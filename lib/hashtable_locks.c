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

#include "hashtable.h"
#include "hash.h"

#define VERSION "0.1"
const char* tablename = "linked-locks/single-probe/remain:V" VERSION;
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
  pthread_rwlock_t resizeLock;
  pthread_rwlock_t swapLock;
  unsigned int seed;
  int TableSize;
  volatile int cur;
} HashTable;


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
int addNode(HashTable* head, block* node, int slot){
  block* cur=head->table[slot];
  while(cur!=NULL){
    if(cur->val==node->val){
      return -1;
    }
    cur=cur->next;
  }
    node->next=head->table[slot];
    head->table[slot]=node;
   return head->cur++;
}

double freeAll(HashTable* head, int last, int verbose){
  while(head->resizing){
    sleep(1);
  }

    int total=0;
    printf("Table[%lu]: %d\n", head->resizing, head->TableSize);
    for(int i =0;i<head->TableSize;i++){
      block* cur=head->table[i];
      if(verbose){
      printf("%d: ", i);
      }
      while(cur!=NULL){
	total++;
	if(verbose){
	printf("[%lu]->", cur->val);
	}
	cur=cur->next;
      }
      if(verbose){
      printf("NULL\n");
      }

    }
  printf("Total Elements: %d\n", total);
  return 0;
}

// see if entry is in the table
int checkTableQuery(HashTable* head, unsigned long val){

  int start=murmur3_32((const uint8_t *)&val, kSize, head->seed)%head->TableSize;
  pthread_rwlock_rdlock(&head->swapLock);
  pthread_rwlock_rdlock(&head->tableLocks[start]);
  block* cur=head->table[start];
  while(cur!=NULL){
    if(val==cur->val){
      pthread_rwlock_unlock(&head->swapLock);
      pthread_rwlock_unlock(&head->tableLocks[start]);

      return 1;
    }
    cur=cur->next;
  }
  pthread_rwlock_unlock(&head->swapLock);
  pthread_rwlock_unlock(&head->tableLocks[start]);
  return 0;
}

// initialize a new main hashtable
HashTable* initTable(HashTable* head, int InitSize, int HashAttempts, int numThreads, unsigned int * seeds){
  head=(HashTable*)calloc(1,sizeof(HashTable));
  head->seed=seeds[0];
  head->TableSize=InitSize;
  head->cur=0;
  head->table=calloc(InitSize, sizeof(block*));;
  head->tableLocks=malloc(sizeof(pthread_rwlock_t)*InitSize);
  for(int i =0;i<InitSize;i++){
    pthread_rwlock_init(&head->tableLocks[i],NULL);
  }
  pthread_rwlock_init(&head->resizeLock,NULL);
  pthread_rwlock_init(&head->swapLock,NULL);
  return head;
}

int insertTable_inner(HashTable* head, int start, block* node);
// return 1 if inserted, 0 if already there
int insertTable(HashTable* head,  int start, entry* ent, int tid){
  unsigned long temp=ent->val;
  block* new_node=(block*)ent;
  new_node->next=NULL;
  new_node->val=temp;
  return insertTable_inner(head, murmur3_32((const uint8_t *)&new_node->val, kSize, head->seed)%head->TableSize, new_node);
}

int insertTable_inner(HashTable* head, int start, block* node){
  pthread_rwlock_rdlock(&head->swapLock);

  pthread_rwlock_wrlock(&head->tableLocks[start]);
  int val=addNode(head, node, start);
  pthread_rwlock_unlock(&head->tableLocks[start]);
  pthread_rwlock_unlock(&head->swapLock);
  if(val==-1){
    return 0;
  }
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



void newTable(HashTable* head){
  //malloc new cache
  block** tempTable=(block**)calloc(head->TableSize<<1,sizeof(block*));


  pthread_rwlock_t* newLocks=(pthread_rwlock_t*)malloc((head->TableSize<<1)*(sizeof(pthread_rwlock_t)));
  for(int i =0;i<(head->TableSize<<1);i++){
    pthread_rwlock_init(&newLocks[i],NULL);
    }
  pthread_rwlock_wrlock(&head->swapLock);
  head->tableLocks=newLocks;
  head->cur=0;
  int prevSize=head->TableSize;
  head->TableSize=head->TableSize<<1;
  block** prevTable=head->table;
  head->table=tempTable;
  pthread_rwlock_unlock(&head->swapLock);
 
  //iterate through old cache starting at tail and add to new cache
  //this will mess up LRU but means that checking old cache in normal order
  //during resizing if an object is not found it will check new cache before requesting
  //info from web
  block* cur;
  block* next;
  for(int i =0;i<prevSize;i++){
    cur=prevTable[i];
    while(cur!=NULL){
      next=cur->next;
      insertTable_inner(head,murmur3_32((const uint8_t *)&cur->val, kSize, head->seed)%head->TableSize, cur);
      cur=next;
    }
  }
  free(prevTable);
  head->resizing=0;
}
