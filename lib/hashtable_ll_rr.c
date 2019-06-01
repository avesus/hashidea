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


#define max_tables 64

//this is linked list row resizing (when row passes max elements) creates new table.
//I believe there is still an obscure race condition that needs to be fixed. Think it
//could also be sped up by preallocating slots

//for the que (part of michael scott que implimentation)
//normally stores counter variable too but to do CAS removed it
//and store in first 4 bits of pointer (max 14 per row, min 2)
typedef struct pointer{
  volatile struct node* ptr;
}pointer;

//where the value is stored, pointers to next (for que)
typedef struct node{
  volatile unsigned long val;
  volatile struct pointer next;
}node;

//que itself, has head and tail
typedef struct que{
  volatile struct pointer head;
  volatile struct pointer tail;
}que;

//sub table stores array of ques (hash table) and table size
typedef struct SubTable{
  que ** InnerTable;
  int TableSize;
} SubTable;

//head of hashtable, has array of subtables (max_table amount)
//stores current max table index, seed for hashing, and max elements
//per row before resizing
typedef struct HashTable{
  SubTable** TableArray;
  int cur;
  unsigned int seed;
  int maxElements;
} HashTable;


//insert table function (insertTable from api calls this as arguments are diffent
//for linked list vs open table hashing)
int insertTable_inner(HashTable* head, node* new_node);

//gets count value from first bits of pointer
int getAmt(volatile node* ptr){
  return ((unsigned long)ptr)&0xf;
}

//gets pointer by anding off the first 4 bits from a ptr
volatile node* getPtr(volatile node* ptr){
  unsigned long mask=0xf;
  return (volatile node*)((((unsigned long)ptr)&(~mask)));
}

//set count value for a pointer
void setPtr(volatile node** ptr, int amt){

  *ptr=(void*)(((unsigned long)getPtr(*ptr))|amt);
}

//increment count value for a pointer
void incPtr(volatile node** ptr){

  int amt=getAmt(*ptr)+1;
  *ptr=(void*)(((unsigned long)getPtr(*ptr))|amt);
}

//free a given SUBTABLE
void freeTable(SubTable* toadd){
  for(int i =0;i<toadd->TableSize;i++){
    free(toadd->InnerTable[i]);
  }
  free(toadd->InnerTable);
  free(toadd);

}

//create a new sub table
SubTable* createTable(int n_size){
  SubTable* t=(SubTable*)malloc(sizeof(SubTable));
  t->TableSize=n_size;
  t->InnerTable=(que**)malloc(sizeof(que*)*(t->TableSize));
  for(int i =0;i<n_size;i++){
    node* new_node = (node*)malloc(sizeof(node));
    new_node->val=-1;
    t->InnerTable[i]=(que*)malloc(sizeof(que));
    new_node->next.ptr=NULL;
    t->InnerTable[i]->head.ptr=new_node;
    t->InnerTable[i]->tail.ptr=new_node;
  }

  return t;
}

//query function, returns 1 if found, 0 otherwise
int checkTableQuery(HashTable* head, unsigned long val){
  SubTable* t;
  for(int i =0;i<head->cur;i++){
    t=head->TableArray[i];
    unsigned int bucket= murmur3_32((const uint8_t *)&val, 4, head->seed)%t->TableSize;
    volatile pointer tail = tail=t->InnerTable[bucket]->head;
    while(getPtr(tail.ptr)!=NULL){
      if(getPtr(tail.ptr)->val==val){
	return 1;
      }
      tail=getPtr(tail.ptr)->next;
    }
  }
  return 0;
}

//trys to add a new sub table to head, uses CAS, loser will free there table
int addDrop(HashTable* head, node* ele ,SubTable* toadd, int tt_size){
  SubTable* expected=NULL;

  //add new sub table to head at tt_size
  int res = __atomic_compare_exchange(&head->TableArray[tt_size] ,&expected, &toadd, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);

  //if win update size then reinsert
  if(res){
    int newSize=tt_size+1;
    __atomic_compare_exchange(&head->cur,
			      &tt_size,
			      &newSize,
			      1,__ATOMIC_RELAXED, __ATOMIC_RELAXED);
    insertTable_inner(head, ele);
  }
  else{
    //if lose free table then update size (just so that neither is waiting for other to update)
    //then reinsert
    freeTable(toadd);
    int newSize=tt_size+1;
    __atomic_compare_exchange(&head->cur,
			      &tt_size,
			      &newSize,
			      1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
    insertTable_inner(head, ele);
  }
  return 0;
}




//makes a ptr from a node and count value
volatile pointer makeFrom(volatile node* new_node, int amt){
  volatile pointer p = {.ptr=new_node};
  setPtr((volatile node**)&(p.ptr),amt);
  return p;
}

//api function, just serves as wrapped for insertTable_inner
int insertTable(HashTable* head,  int start, entry* ent, int tid){
  node* new_node = (node*)malloc(sizeof(node));
  new_node->next.ptr=NULL;
  new_node->val=ent->val;
  free(ent);
  return insertTable_inner(head, new_node);
}


//the actual insert function, returns 1 if succesfully inserted, 0 otherwise (i.e if found)
int insertTable_inner(HashTable* head, node* new_node){
  int startCur=head->cur;
  SubTable* t;

  //iterate through all subtables in head
  for(int i =0;i<head->cur;i++){
    t=head->TableArray[i];

    //get bucket
    unsigned int bucket= murmur3_32((const uint8_t *)&new_node->val, 4, head->seed)%t->TableSize;
    volatile pointer tail;
    volatile pointer next;

    //start from head of que
    tail=t->InnerTable[bucket]->head;

    //bool to see if should try and reset tail after break
    int should_hit=0;

    //keep trying to add unless break or return
    while(1){

      //check existing que, tail value from before will be actual tail at end
      while(getPtr(tail.ptr)!=getPtr(t->InnerTable[bucket]->tail.ptr)){
	if(getPtr(tail.ptr)->val==getPtr(new_node)->val){
	  return 0;
	}
	tail=getPtr(tail.ptr)->next;
      }
      next=getPtr(tail.ptr)->next;
      if(getPtr(tail.ptr)->val==getPtr(new_node)->val){
	return 0;
      }

      //check tail still is real tail, if is move on
      if(getPtr(tail.ptr)==getPtr(t->InnerTable[bucket]->tail.ptr)){

	//make sure new tail is not trying to be added
	if(getPtr(next.ptr)==NULL){

	  //get count of row
	  int pval=getAmt(tail.ptr)+1;

	  //if count is out of bounds break and go to next table
	  //my guess is that the race condition is here
	  if(pval>=head->maxElements){

	    //if tail is still real tail then can just break (nothing new was added)
	    if(getPtr(tail.ptr)==getPtr(t->InnerTable[bucket]->tail.ptr)){
	      break;
	    }

	    //otherwise recheck make sure not in there (dont need to do full check will update
	    //once find race condition)
	    tail=t->InnerTable[bucket]->head;
	    while(getPtr(tail.ptr)!=getPtr(t->InnerTable[bucket]->tail.ptr)){
	      if(getPtr(tail.ptr)->val==getPtr(new_node)->val){
		return 0;
	      }
	      tail=getPtr(tail.ptr)->next;
	    }
	    next=getPtr(tail.ptr)->next;
	    if(getPtr(tail.ptr)->val==getPtr(new_node)->val){
	      return 0;
	    }
	    break;
	  }

	  //if still good try and add
	  volatile pointer p=makeFrom(new_node, pval);
	  if(__atomic_compare_exchange((pointer*)&(getPtr(tail.ptr)->next) ,(pointer*)&next,(pointer*) &p, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)){

	    //if succeed set should_hit (so fixes tail) and break
	    should_hit=1;
	    break;
	  }
	  else{

	    //if failed reupdate next
	    if(next.ptr->val==new_node->val){
	      return 0;
	    }

	    //update next
	    int pval=getAmt(next.ptr);

	    volatile pointer p2=makeFrom(next.ptr, pval);
	    if(__atomic_compare_exchange((pointer*)&t->InnerTable[bucket]->tail ,(pointer*)&tail, (pointer*)&p2, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)){
	    }
	  }
	}
      }
    }

    //if should hit try and update tail
    if(should_hit){
      int pval=getAmt(t->InnerTable[bucket]->tail.ptr)+1;
      volatile pointer p3=makeFrom(new_node, pval);
      if(__atomic_compare_exchange((pointer*)&t->InnerTable[bucket]->tail ,(pointer*)&tail, (pointer*)&p3, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)){
      }
      return 0;
    }

    //if break go to next sub table (break and not add)
    startCur=head->cur;
  }

  //if didnt find free slot in entire sub table add a new one
  SubTable* new_table=createTable(head->TableArray[startCur-1]->TableSize<<1);
  addDrop(head, new_node, new_table, startCur);
}


//initilize the head of table (using hash attempts as maxElements per row)
HashTable* initTable(HashTable* head, int InitSize, int HashAttempts, int numThreads, unsigned int* seeds){
  head=(HashTable*)malloc(sizeof(HashTable));
  head->TableArray=(SubTable**)malloc(max_tables*sizeof(SubTable*));
  head->cur=1;
  head->seed=*seeds;
  head->maxElements=HashAttempts;
  head->TableArray[0]=createTable(InitSize);
  return head;
}

//free all and returns total items in table/total sub table slots allocated
double freeAll(HashTable* head, int last, int verbose){
  int amt=0;
  SubTable* t;
  int extra=0;
  int  t_amt=0;
  double total=0;
  for(int j =0;j<head->cur;j++){
    t=head->TableArray[j];
    total+=t->TableSize;
    if(verbose){
      printf("Table %d Size ", j);
    }
    extra+=t->TableSize;
    for(int i =0;i<t->TableSize;i++){
      volatile node* temp=getPtr(t->InnerTable[i]->head.ptr);

      int pval=getAmt(temp->next.ptr);
      temp=getPtr(temp->next.ptr);

      while(temp!=NULL){

	amt++;
	t_amt++;
	temp=getPtr(temp->next.ptr);

      }
    }
    if(verbose){
      printf("%d/%d\n", t_amt, t->TableSize);
    }
    t_amt=0;
  }

  printf("amt=%d\n", amt);
  return ((double)amt)/total;
}


//not needed just to finish api
int getStart(HashTable* head){
  return 0;
}
