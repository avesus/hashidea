#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>
#include "cache-params.h"
#include "hashtable.h"
#include "hash.h"

#define VERSION "0.1"
const char* tablename = "linked-locks/single-probe/move:V" VERSION;
const char* shortname = "OMR:V" VERSION;



typedef struct block{
  unsigned long val;
  struct block * next;
}block;


//a sub table (this should be hidden)
typedef struct SubTable {
  entry** InnerTable; //rows (table itself)
  int TableSize; //size
} SubTable;

// head of cache: this is the main hahstable
typedef struct HashTable{
  block** table;
  volatile unsigned long resizing;
  pthread_rwlock_t* tableLocks;
  pthread_rwlock_t swapLock;
  unsigned int seed;
  int TableSize;
  volatile int cur;
} HashTable;

#include "cache-constants.h"

#define max_tables 64 //max tables to create

//return values for checking table.  Returned by lookupQuery
#define kSize 4
#define notIn -3 
#define in -1
#define unk -2
#define min(X, Y)  ((X) < (Y) ? (X) : (Y))
#define max(X, Y)  ((X) < (Y) ? (Y) : (X))
// create a sub table
// free hash table when done
void newTable(HashTable* head);


//add nodes to ll for a given slot
int addNode(HashTable* head, block* node, int slot){

  //if slot is null just add
  if(!head->table[slot]){
  pthread_rwlock_wrlock(&head->tableLocks[slot]);

  //after acquiring lock if still null add, else unlock
  if(!head->table[slot]){
    head->table[slot]=node;
    pthread_rwlock_unlock(&head->tableLocks[slot]);
    //return approx of amount of elements in table (for resizing)
    return head->cur++;
  }
  else{

    //else unlock (maybe should just iterate here for end so not much lock/unlocking).
    pthread_rwlock_unlock(&head->tableLocks[slot]);
  }

  }

  //iterate from start to end checking vals
  block* cur=head->table[slot];
  block* p=NULL;
  while(cur!=NULL){
    if(cur->val==node->val){
      return -1;
    }
    p=cur;
    cur=cur->next;
  }
  //didnt find self
  pthread_rwlock_wrlock(&head->tableLocks[slot]);

  //iterate through remaining possibly values between last iteration and locking
  //(doesnt repeat all items unless 1 item..
  cur=p;
  while(cur!=NULL){
    if(cur->val==node->val){
      pthread_rwlock_unlock(&head->tableLocks[slot]);
      return -1;
    }
    p=cur;
    cur=cur->next;
  }

  //add item
  p->next=node;
  pthread_rwlock_unlock(&head->tableLocks[slot]);
  //return approx of amt
  return head->cur++;
}

double freeAll(HashTable* head, int last, int verbose){

    int total=0;
    if(verbose){
    printf("Table[%lu]: %d\n", head->resizing, head->TableSize);
    }
    for(int i =0;i<head->TableSize;i++){
      block* cur=head->table[i];
      if(verbose){
	//      printf("%d: ", i);
      }
      while(cur!=NULL){
	total++;
	if(verbose){
	  //printf("[%lu]->", cur->val);
	}
	cur=cur->next;
      }
      if(verbose){
	// printf("NULL\n");
      }

    }
    if(verbose){
  printf("Total Elements: %d\n", total);
    }
    free(head->tableLocks);
    free(head->table);
  return 0;
}

// see if entry is in the table
int checkTableQuery(HashTable* head, unsigned long val){

  //just itrate table
    pthread_rwlock_rdlock(&head->swapLock);
    unsigned int start=murmur3_32((const uint8_t *)&val, kSize, head->seed)%head->TableSize;
    //pthread_rwlock_rdlock(&head->tableLocks[start]);
  block* cur=head->table[start];
  while(cur!=NULL){
    if(val==cur->val){

      //  pthread_rwlock_unlock(&head->tableLocks[start]);
      pthread_rwlock_unlock(&head->swapLock);
      return 1;
    }
    cur=cur->next;
  }

  //  pthread_rwlock_unlock(&head->tableLocks[start]);
  pthread_rwlock_unlock(&head->swapLock);
  return 0;
}

// initialize a new main hashtable
HashTable* initTable(HashTable* head, int InitSize, int HashAttempts, int numThreads, unsigned int * seeds,double lines){
  head=(HashTable*)calloc(1,sizeof(HashTable));
  head->seed=seeds[0];
  head->TableSize=InitSize;
  head->cur=0;
  head->table=(block**)calloc(InitSize, sizeof(block*));;
  head->tableLocks=(pthread_rwlock_t*)malloc(sizeof(pthread_rwlock_t)*InitSize);
  for(int i =0;i<InitSize;i++){
    pthread_rwlock_init(&head->tableLocks[i],NULL);
  }
  pthread_rwlock_init(&head->swapLock,NULL);
  return head;
}

int insertTable_inner(HashTable* head, block* node);
// return 1 if inserted, 0 if already there
int insertTable(HashTable* head,  int start, entry* ent, int tid){
  unsigned long temp=ent->val;
  block* new_node=(block*)ent;
  new_node->next=NULL;
  new_node->val=temp;
  return insertTable_inner(head, new_node);
}

int insertTable_inner(HashTable* head, block* node){
 
  pthread_rwlock_rdlock(&head->swapLock);
  unsigned int start=murmur3_32((const uint8_t *)&node->val, kSize, head->seed)%head->TableSize;
  int val=addNode(head, node, start);
  pthread_rwlock_unlock(&head->swapLock);
  if(val==-1){
    return 0;
  }

  //return is approx table amt, if amt>size (lf>1) and not currently resizing cas for
  //resizing bool and do it
  if((!head->resizing)&&val>head->TableSize){
      unsigned long ex=0;
      unsigned long n=1;
      int res = __atomic_compare_exchange(&head->resizing ,&ex, &n, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
      if(res){
		newTable(head);
      }
    }
  return 1;
}





int getStart(HashTable* head){
  return 0;
}


int deleteVal(HashTable* head, unsigned long val){
  return 0;
}
////////////////////////////////////////////////////////////////
// used only for testing and timing


//creates new table size 2x
void newTable(HashTable* head){

  //alloc new table and locks
  block** tempTable=(block**)calloc(head->TableSize<<1,sizeof(block*));


  pthread_rwlock_t* newLocks=(pthread_rwlock_t*)malloc((head->TableSize<<1)*(sizeof(pthread_rwlock_t)));
  for(int i =0;i<(head->TableSize<<1);i++){
    pthread_rwlock_init(&newLocks[i],NULL);
    }

  //actually change the values stored at head 
  pthread_rwlock_wrlock(&head->swapLock);
  head->tableLocks=newLocks;
  head->cur=0;
  int prevSize=head->TableSize;
  head->TableSize=head->TableSize<<1;
  block** prevTable=head->table;
  head->table=tempTable;
  pthread_rwlock_unlock(&head->swapLock);
 
  //iterate through old table re add all (gunna really fuck up one thread).Since hashattempts
  //is unused maybe hashattempts as bool for whether to resize at all or not?
  block* cur;
  block* next;
  for(int i =0;i<prevSize;i++){
    cur=prevTable[i];
    while(cur!=NULL){
      next=cur->next;
      cur->next=NULL;
      insertTable_inner(head, cur);
      cur=next;
    }
  }
  free(prevTable);
  head->resizing=0;

}
