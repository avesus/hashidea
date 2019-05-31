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



typedef struct pointer{
  volatile struct node* ptr;
}pointer;

typedef struct node{
  volatile unsigned long val;
  volatile struct pointer next;
}node;


typedef struct que{
  volatile struct pointer head;
  volatile struct pointer tail;
}que;

typedef struct SubTable{
  que ** InnerTable;
  int TableSize;
} SubTable;


typedef struct HashTable{
  SubTable** TableArray;
  int cur;
  unsigned int seed;
  int maxElements;
} HashTable;


int insertTable_inner(HashTable* head, node* new_node);

int getAmt(volatile node* ptr){
  return ((unsigned long)ptr)&0xf;
}
volatile node* getPtr(volatile node* ptr){
  unsigned long mask=0xf;
  return (volatile node*)((((unsigned long)ptr)&(~mask)));
}
void setPtr(volatile node** ptr, int amt){

  *ptr=(void*)(((unsigned long)getPtr(*ptr))|amt);
}
void incPtr(volatile node** ptr){

  int amt=getAmt(*ptr)+1;
  *ptr=(void*)(((unsigned long)getPtr(*ptr))|amt);
}

void freeTable(SubTable* toadd){
  for(int i =0;i<toadd->TableSize;i++){
    free(toadd->InnerTable[i]);
  }
  free(toadd->InnerTable);
  free(toadd);

}
SubTable* createTable(int n_size){
  SubTable* t=(SubTable*)malloc(sizeof(SubTable));
  t->TableSize=n_size;
  t->InnerTable=(que**)malloc(sizeof(que*)*(t->TableSize));
  //  ht->next=NULL;
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
int checkTableQuery(HashTable* head, unsigned long val){
  //  printf("start search\n");
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


int addDrop(HashTable* head, node* ele ,SubTable* toadd, int tt_size){
  SubTable* expected=NULL;
  int res = __atomic_compare_exchange(&head->TableArray[tt_size] ,&expected, &toadd, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
  if(res){
    int newSize=tt_size+1;
    __atomic_compare_exchange(&head->cur,
			      &tt_size,
			      &newSize,
			      1,__ATOMIC_RELAXED, __ATOMIC_RELAXED);
    insertTable_inner(head, ele);
  }
  else{
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





volatile pointer makeFrom(volatile node* new_node, int amt){
  volatile pointer p = {.ptr=new_node};
  setPtr((volatile node**)&(p.ptr),amt);
  return p;
}


int insertTable(HashTable* head,  int start, entry* ent, int tid){
  node* new_node = (node*)malloc(sizeof(node));
  new_node->next.ptr=NULL;
  new_node->val=ent->val;
  free(ent);
  return insertTable_inner(head, new_node);
}


int insertTable_inner(HashTable* head, node* new_node){
  int startCur=head->cur;
  SubTable* t;
  for(int i =0;i<head->cur;i++){
    t=head->TableArray[i];
    unsigned int bucket= murmur3_32((const uint8_t *)&new_node->val, 4, head->seed)%t->TableSize;

    volatile pointer tail;
    volatile pointer next;
    tail=t->InnerTable[bucket]->head;

    int should_hit=0;
    while(1){
    
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


      if(getPtr(tail.ptr)==getPtr(t->InnerTable[bucket]->tail.ptr)){
	if(getPtr(next.ptr)==NULL){
	  int pval=getAmt(tail.ptr)+1;
	  if(pval>=head->maxElements){
	    if(getPtr(tail.ptr)==getPtr(t->InnerTable[bucket]->tail.ptr)){
	      break;
	    }
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
	  volatile pointer p=makeFrom(new_node, pval);
	  if(__atomic_compare_exchange((pointer*)&(getPtr(tail.ptr)->next) ,(pointer*)&next,(pointer*) &p, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)){
	    should_hit=1;
	    break;
	  }
	  else{
	    if(next.ptr->val==new_node->val){
	      return 0;
	    }
	    int pval=getAmt(next.ptr);

	    volatile pointer p2=makeFrom(next.ptr, pval);
	    if(__atomic_compare_exchange((pointer*)&t->InnerTable[bucket]->tail ,(pointer*)&tail, (pointer*)&p2, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)){
	    }
	  }
	}
      }
    }
    if(should_hit){
      int pval=getAmt(t->InnerTable[bucket]->tail.ptr)+1;
      volatile pointer p3=makeFrom(new_node, pval);
      if(__atomic_compare_exchange((pointer*)&t->InnerTable[bucket]->tail ,(pointer*)&tail, (pointer*)&p3, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)){
      }
      return 0;
    }
	 
    startCur=head->cur;
  }
  SubTable* new_table=createTable(head->TableArray[startCur-1]->TableSize<<1);
  addDrop(head, new_node, new_table, startCur);
}

HashTable* initTable(HashTable* head, int InitSize, int HashAttempts, int numThreads, unsigned int* seeds){
  head=(HashTable*)malloc(sizeof(HashTable));
  head->TableArray=(SubTable**)malloc(max_tables*sizeof(SubTable*));
  head->cur=1;
  head->seed=*seeds;
  head->maxElements=HashAttempts;
  head->TableArray[0]=createTable(InitSize);
  return head;
}
  
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

int getStart(HashTable* head){
  return 0;
}
