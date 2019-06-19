#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>


#include "hashtable.h"
#include "hash.h"

#define VERSION "0.1"
const char* tablename = "open/multiprobe/remain:V" VERSION;

#define resizeShift 1
#define max_tables 64

//no race condition but slow. Resizes when total elements in table passes some threshold


//ptr struct for que (needs count variable to impliment delete (also need hazard ptrs and to
//leave a seperate thread for cleanup (basically need share_ptr))
typedef struct pointer{
  volatile struct node* ptr;

}pointer;


//not where actual stuff is stored
typedef struct node{
  volatile unsigned long val;
  volatile struct pointer next;
}node;

//the list itself
typedef struct que{
  volatile struct pointer head;
  volatile struct pointer tail;
}que;


//sub table, has array of ques, a size, and estimate for current elements (totalelements is not
//accurate as added to by multiple threads)
typedef struct SubTable{
  que ** InnerTable;
  int * threadCopy;
  int TableSize;
  int TotalElements;
} SubTable;


//head of hash table
typedef struct HashTable{
  SubTable** TableArray;
  volatile unsigned long start;
  int cur;

  int numThreads;
  unsigned int seed;
  int maxElements;
} HashTable;




int getBool(que* ent){
  return ((unsigned long)ent)&1;
}
que* getPtr(que* ent){
  unsigned long mask=1;
  return (que*)(((unsigned long)ent)&(~mask));
}

int setPtr(que** ent){
  que* newEnt=(que*)((unsigned long)(*ent)|1);
  que* exEnt= getPtr(*ent);
  return __atomic_compare_exchange(ent,&exEnt, &newEnt, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}


int sumArr(  int* arr, int size){
  int sum=0;
  for(int i =0;i<size;i++){
    sum+=arr[i];
  }
  return sum;
}


//actual insert function used (use api insertTable as wrapper as arguments needed differ).
//b is the bucket to be added (-1 for most things, will be a real value when adding a final
//node to a row
int insertTable_inner(HashTable* head, node* new_node, int start, int tid, int b);

//frees a sub table
void freeTable(SubTable* toadd){
  /*for(int i =0;i<toadd->TableSize;i++){
    free((void*)toadd->InnerTable[i]->head.ptr);
    free(toadd->InnerTable[i]);
    }*/
  free(toadd->threadCopy);
  free(toadd->InnerTable);
  free(toadd);

}

//creates a sub table
SubTable* createTable(HashTable* head, int n_size){
  printf("%lu --- %d ----\n",head->start, n_size);
  SubTable* t=(SubTable*)malloc(sizeof(SubTable));
  t->TableSize=n_size;
  t->TotalElements=0;
  t->threadCopy=( int*)calloc(head->numThreads,sizeof(int));
  unsigned long chunkSize=(sizeof(node)+sizeof(que)+sizeof(que*))*(t->TableSize);
  unsigned char* memChunk=(unsigned char*)malloc(chunkSize);

  t->InnerTable=(que**)memChunk;
  unsigned long nodeOffset=n_size*sizeof(que*);
  unsigned long queOffset=nodeOffset+n_size*sizeof(node);
  
  //get rid of -1 and chunk up the memory
  for(int i =0;i<n_size;i++){
    node* new_node = (node*)(memChunk+nodeOffset+(i<<4));//(node*)malloc(sizeof(node));
    new_node->val=-1;
    t->InnerTable[i]=(que*)(memChunk+queOffset+(i<<4));//malloc(sizeof(que));
    new_node->next.ptr=NULL;
    t->InnerTable[i]->head.ptr=new_node;
    t->InnerTable[i]->tail.ptr=new_node;
  }
  printf("%lu ----%d ---\n", head->start, n_size);
  return t;
}

//checks table for a value, 1 means is in, 0 for not in
int checkTableQuery(HashTable* head, unsigned long val){
  SubTable* t;
  for(int i =0;i<head->cur;i++){
    t=head->TableArray[i];
    unsigned int bucket= murmur3_32((const uint8_t *)&val, 4, head->seed)%t->TableSize;
    volatile pointer tail = tail=getPtr(t->InnerTable[bucket])->head;
    while(tail.ptr!=NULL){
      if(tail.ptr->val==val){
	return 1;
      }
      tail=tail.ptr->next;
    }
  }
  return 0;
}

//add drop (same as in open table)
int addDrop(HashTable* head, node* ele ,SubTable* toadd, int tt_size, int tid){
  SubTable* expected=NULL;
  int res = __atomic_compare_exchange(&head->TableArray[tt_size] ,&expected, &toadd, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
  if(res){
    int newSize=tt_size+1;
    __atomic_compare_exchange(&head->cur,
			      &tt_size,
			      &newSize,
			      1,__ATOMIC_RELAXED, __ATOMIC_RELAXED);
    insertTable_inner(head,ele,0,tid,-1);
  }
  else{
    freeTable(toadd);
    int newSize=tt_size+1;
    __atomic_compare_exchange(&head->cur,
			      &tt_size,
			      &newSize,
			      1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
    insertTable_inner(head,ele,0,tid, -1);
  }
  return 0;
}


//makes a ptr from a node
volatile pointer makeFrom(volatile node* new_node){
  volatile pointer p = {.ptr=new_node};
  return p;
}

//insert function from API (wrapped for insertTable_inner)
int insertTable(HashTable* head,  int start, entry* ent, int tid){
  node* new_node = (node*)malloc(sizeof(node));
  new_node->next.ptr=NULL;
  new_node->val=ent->val;
  //  free(ent);
  return insertTable_inner(head,  new_node, start, tid, -1);
}


//actual insert function
int insertTable_inner(HashTable* head, node* new_node, int start, int tid, int b){
  int startCur=head->cur;
  SubTable* t;
  int pre_resize=0;
  int do_return=0;
  //iterate through all sub tables
  for(int i =start;i<head->cur;i++){
    t=head->TableArray[i];
    unsigned int bucket;
    int doCopy_real=0;
    int doCopy=(i==head->start)&&(head->cur-head->start>2&&t->TotalElements>(head->maxElements*t->TableSize));
    //if b is not -1 basically will use it for table (when adding a 0 at end of row
    //after resizing (otherwise race condition)
    if(b>=0){
      bucket=b;
    }
    else{
      //otherwise get bucket
      bucket= murmur3_32((const uint8_t *)&new_node->val, 4, head->seed)%t->TableSize;

    }
    int before=getBool(t->InnerTable[bucket]);
    if(doCopy&&new_node->val){

	doCopy_real=setPtr(&t->InnerTable[bucket]);
	printf("----------------------START-------------------------\n");
	printf("%d[%d]: %d|%d\n", i, bucket,doCopy_real, before);

      }
    volatile pointer tail;
    volatile pointer next;
    unsigned long exStart=head->start;
    unsigned long newStart=exStart+1;

    //will be local version of tail, starts at head
    tail=getPtr(t->InnerTable[bucket])->head;
    int should_hit=0;
    while(1){
      //check existing que for value, if not found tail will equal actual
      //tail after this check
      while(tail.ptr!=getPtr(t->InnerTable[bucket])->tail.ptr){
	if(tail.ptr->val==new_node->val){
	    if(!doCopy_real){
	    return 0;
	    }
	    else{
	      do_return=1;
	    }
	}
	if(doCopy&&doCopy_real&&tail.ptr->val){
	    //add item to next sub table
	    node* carry_node=(node*)malloc(sizeof(node));
	    carry_node->next.ptr=NULL;
	    carry_node->val=tail.ptr->val;
	    insertTable_inner(head, carry_node, head->start+1, tid, -1);

	}
	tail=tail.ptr->next;
      }
      if(doCopy&&doCopy_real&&new_node->val){
	int sumB=0;
	for(int n=0;n<t->TableSize;n++){
	  sumB+=getBool(t->InnerTable[n]);
	}
		printf("%d[%d]: %d|%d == %d = %d vs %d\n", i, bucket,doCopy_real, before,sumArr(t->threadCopy, head->numThreads),sumB, t->TotalElements );
	t->threadCopy[tid]++;
	sumB=0;
	for(int n=0;n<t->TableSize;n++){
	  sumB+=getBool(t->InnerTable[n]);
	}
	printf("%d[%d]: %d|%d == %d = %d vs %d\n", i, bucket,doCopy_real, before,sumArr(t->threadCopy, head->numThreads),sumB, t->TotalElements );
	printf("----------------------END-------------------------\n");

	if(t->TableSize==sumArr(t->threadCopy, head->numThreads)){
	  __atomic_compare_exchange(&head->start,&exStart, &newStart, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
	  
	}
      }
      if(do_return){
	return 0;
      }      
      next=tail.ptr->next;
      if(tail.ptr->val==new_node->val){
	if(new_node->val!=-1||tail.ptr==getPtr(t->InnerTable[bucket])->head.ptr){
	  return 0;
	}
      }
      //if has a 0 value means cant add anymore to this row
      if(!getPtr(t->InnerTable[bucket])->tail.ptr->val){
	if(new_node->val){
	  break;
	}
	else{
	  return 0;
	}
      }
      //if total elements passes threshhold then add a 0 node (to make it so nothing new can
      //be added to this row, otherwise ABA is race condition)
      if(t->TotalElements>(head->maxElements*t->TableSize)&&new_node->val){
	node* end_node = (node*)malloc(sizeof(node));
	end_node->next.ptr=NULL;
	end_node->val=0;
	insertTable_inner(head,end_node, i, tid, bucket);
      }
      //the acutal adding to the table (just normal michael scott que code)
      if(tail.ptr==getPtr(t->InnerTable[bucket])->tail.ptr){
	if(next.ptr==NULL){
	  volatile pointer p=makeFrom(new_node);
	  if(__atomic_compare_exchange((pointer*)&(tail.ptr->next) ,(pointer*)&next,(pointer*) &p, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)){
	    should_hit=1;

	    //increment total elements (just estimate is enough)
	    if(new_node->val){
	    pre_resize=t->TotalElements++;
	    }
	    break;
	  }
	  else{
	    if(next.ptr->val==new_node->val){
	      if(new_node->val!=-1||tail.ptr==getPtr(t->InnerTable[bucket])->head.ptr){
		return 0;
	      }
	    }

	    volatile pointer p2=makeFrom(next.ptr);
	    if(__atomic_compare_exchange((pointer*)&getPtr(t->InnerTable[bucket])->tail ,(pointer*)&tail, (pointer*)&p2, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)){
	    }
	  }
	}
      }
    }
    //if break was after an add try and fix tail
    if(should_hit){
      volatile pointer p3=makeFrom(new_node);
      if(__atomic_compare_exchange((pointer*)&getPtr(t->InnerTable[bucket])->tail ,(pointer*)&tail, (pointer*)&p3, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)){
      }
      //basically if table is exactly some percent allocate next one. This reduces
      //the amount of failed add drop as basically all threads will enter it as totalelements>totalsize
      //is pretty much universal. This is a slight improvement when lots of new tables are being added
      startCur=head->cur;
      if(pre_resize==((head->maxElements*t->TableSize)>>resizeShift)&&new_node->val){
	SubTable* new_table=createTable(head, head->TableArray[startCur-1]->TableSize<<1);
	addDrop(head, new_node, new_table, startCur, tid);
      }
  
      return 1;
    }
 
    startCur=head->cur;
  }

  //if no available slot create next table
    SubTable* new_table=createTable(head, head->TableArray[startCur-1]->TableSize<<1);
  addDrop(head, new_node, new_table, startCur, tid);
}


//init hashtable, hashattempts will be ratio of total elements to table size (needs to be float)
HashTable* initTable(HashTable* head, int InitSize, int HashAttempts, int numThreads, unsigned int* seeds){
  head=(HashTable*)malloc(sizeof(HashTable));
  head->TableArray=(SubTable**)malloc(max_tables*sizeof(SubTable*));
  head->cur=1;
  head->numThreads=numThreads;
  head->start=0;
  head->seed=*seeds;
  head->maxElements=HashAttempts;
  head->TableArray[0]=createTable(head, InitSize);
  return head;
}


//free all and returns total elements/bucket slots
double freeAll(HashTable* head, int last, int verbose){
  int amt=0;
  SubTable* t;
  int extra=0;
  int  t_amt=0;
  double total=0;
  for(int j =0;j<head->cur;j++){
    if(verbose){
    if(j==head->start){
      printf("-> ");
    }
    }
    t=head->TableArray[j];
    total+=t->TableSize;
    if(verbose){
      printf("Table %d Size ", j);
    }
    extra+=t->TableSize;
    for(int i =0;i<t->TableSize;i++){
      if(!getBool(t->InnerTable[i])){
      volatile node* temp=getPtr(t->InnerTable[i])->head.ptr;
      temp=temp->next.ptr;

      while(temp!=NULL){
	if(temp->val){
	amt++;
	t_amt++;
	}
	temp=temp->next.ptr;

      }
      }
    }
    if(verbose){
      int sumB=0;
      for(int n=0;n<t->TableSize;n++){
	sumB+=getBool(t->InnerTable[n]);
      }
      printf("%d/%d - %d/%d\n", t_amt, t->TableSize, sumB, sumArr(t->threadCopy, head->numThreads));
    }
    t_amt=0;
  }

  printf("amt=%d\n", amt);
  return ((double)amt)/total;
}

int getStart(HashTable* head){
  return (int)head->start;
}
