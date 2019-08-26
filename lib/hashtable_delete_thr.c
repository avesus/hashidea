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
const char* tablename = "open/multiprobe/remain:V" VERSION;
const char* shortname = "OMR:V" VERSION;


//a sub table (this should be hidden)
typedef struct SubTable {
  entry** InnerTable; //rows (table itself)
  volatile unsigned long bDel; //being deleted
  int tDeleted; //total elements deleted (estimate)
  int TableSize; //size
  
} SubTable;

// head of cache: this is the main hahstable
typedef struct HashTable{
  pthread_cond_t dCond;
  pthread_mutex_t dMutex;
  pthread_mutex_t fMutex;
  pthread_t dThread;
  volatile unsigned long deleting;
  SubTable** TableArray; //array of tables
  unsigned int * seeds;
  volatile int stopResize;
  int delIndex;
  int hashAttempts;
  int cur; //current max index (max exclusive)
  int start;
  
} HashTable;


#define max_tables 64 //max tables to create
#define dClear 2
//return values for checking table.  Returned by lookupQuery
#define kSize 4
#define notIn -3 
#define in -1
#define unk -2
#define deleted 1

#define min(X, Y)  ((X) < (Y) ? (X) : (Y))
#define max(X, Y)  ((X) < (Y) ? (Y) : (X))
// create a sub table
static SubTable* createTable(int hsize);
// free a subtable 
static void freeTable(SubTable* table);

//creates new hashtable in the tablearray
static int addDrop(HashTable* head, SubTable* toadd, int AddSlot, entry* ent);

//lookup function in insertTrial to check a given inner table
static int lookup(SubTable* ht, entry* ent, unsigned int s);



int getBool(entry* ptr){
  return ((unsigned long)ptr)&deleted;
}

//gets pointer by anding off the first 4 bits from a ptr
entry* getPtr(entry* ptr){
  unsigned long mask=deleted;
  return (entry*)((((unsigned long)ptr)&(~mask)));
}

//set count value for a pointer
int setPtr(entry** ptr){
  entry* newEnt=(entry*)((unsigned long)(*ptr)|deleted);
  entry* exEnt= getPtr(*ptr);
  return __atomic_compare_exchange(ptr,&exEnt, &newEnt, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}



static int 
lookupQuery(SubTable* ht, unsigned long val, unsigned int s){

  if(ht->InnerTable[s]==NULL){
    return notIn;
  }
  else if(val==getPtr(ht->InnerTable[s])->val){
    if(getBool(ht->InnerTable[s])){
      return notIn;
    }
    return s;
  }
  return unk;
}

int checkTableQuery(HashTable* head, unsigned long val){
  SubTable* ht=NULL;
  unsigned int buckets[head->hashAttempts];
  for(int i =0;i<head->hashAttempts;i++){
    buckets[i]=murmur3_32((const uint8_t *)&val, kSize, head->seeds[i]);
  }
  for(int j=head->start;j<head->cur;j++){

    ht=head->TableArray[j];
        if(ht->bDel==2){
      continue;
    }
    for(int i =0; i<head->hashAttempts; i++) {
      int res=lookupQuery(ht, val, buckets[i]%ht->TableSize);
      if(res==unk){ //unkown if in our not
	continue;
      }
      if(res==notIn){ //is in
	return 0;
      }
      //not in (we got null so it would have been there)
      return 1;
    }
  }
  return 0;
}


double freeAll(HashTable* head, int last, int verbose){
  head->stopResize=1;
  pthread_mutex_lock(&head->fMutex);
  SubTable* ht=NULL;
  double count=0;
  double totalSize=0;
  int * items=NULL;
  if(verbose){
    items=(int*)calloc(sizeof(int), head->cur);
    printf("Tables:\n");
  }
  for(int i = 0;i<head->cur; i++){
    ht=head->TableArray[i];
    if(ht->bDel==2){
      continue;
    }
    totalSize+=ht->TableSize;
    for(int j =0;j<ht->TableSize;j++){
      if(ht->InnerTable[j]!=NULL&&!getBool(ht->InnerTable[j])){
	//free(ht->InnerTable[j]);
	count++;
	if(verbose){
	  items[i]++;
	}
      }
    }
    if(verbose){
      printf("%d: %d/%d -> %lu[%d]\n",i, items[i], ht->TableSize, ht->bDel, ht->tDeleted);
    }
    free(ht->InnerTable);
    free(ht);
  }
  
  free(head->TableArray);

  if(verbose){
    printf("Total: %d\n", (int)count);
    free(items);
  }


  free(head);
  return count/totalSize;  
}

static void 
freeTable(SubTable* ht){
  free(ht->InnerTable);
  free(ht);
}


//check if entry for a given hashing vector is in a table
static int lookup(SubTable* ht, entry* ent, unsigned int s){

  if(ht->InnerTable[s]==NULL){
    return s;
  }
  else if(getPtr(ht->InnerTable[s])->val==ent->val){
    if(getBool(ht->InnerTable[s])){
      return s;
      }
    return in;
  }
  return unk;
}



static int addDrop(HashTable* head, SubTable* toadd, int AddSlot, entry* ent){
  SubTable* expected=NULL;
  int res = __atomic_compare_exchange(&head->TableArray[AddSlot] ,&expected, &toadd, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
  if(res){
    int newSize=AddSlot+1;
    __atomic_compare_exchange(&head->cur,
			      &AddSlot,
			      &newSize,
			      1,__ATOMIC_RELAXED, __ATOMIC_RELAXED);
  }
  else{
    freeTable(toadd);
    int newSize=AddSlot+1;
    __atomic_compare_exchange(&head->cur,
			      &AddSlot,
			      &newSize,
			      1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);

  }
  return 0;
}




int deleteVal(HashTable* head, unsigned long val){
  SubTable* ht=NULL;
  unsigned int buckets[head->hashAttempts];
  for(int i =0;i<head->hashAttempts;i++){
    buckets[i]=murmur3_32((const uint8_t *)&val, kSize, head->seeds[i]);
  }
  for(int j=head->start;j<head->cur;j++){
  
    ht=head->TableArray[j];
    if(ht->bDel==2){
      continue;
    }
    for(int i =0; i<head->hashAttempts; i++) {

      int res=lookupQuery(ht, val, buckets[i]%ht->TableSize);
      if(res==unk){ //unkown if in our not
	continue;
      }
      if(res==notIn){ //is in
	return 0;
      }

      int ret=setPtr(&ht->InnerTable[res]);
      if(ret&&!ht->bDel){
	ht->tDeleted++;
	if(ht->tDeleted>(ht->TableSize>>dClear)&&j!=(head->cur-1)&&!head->deleting){
	  unsigned long expec=0, newv=1;
	  int cret=__atomic_compare_exchange(&head->deleting,&expec, &newv, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
	  if(cret){
	    ht->bDel=1;
	    head->delIndex=j;

	    pthread_cond_signal(&head->dCond);
	  }
	}
      }
      return ret; 
    }
  }
  return 0;
}


int insertTable(HashTable* head,  int start, entry* ent, int tid){
  SubTable* ht=NULL;
  int LocalCur=head->cur;
  unsigned int buckets[head->hashAttempts];
  for(int i =0;i<head->hashAttempts;i++){
	buckets[i]=murmur3_32((const uint8_t *)&ent->val, kSize, head->seeds[i]);    
  }
  while(1){
    for(int j=start;j<head->cur;j++){
    ht=head->TableArray[j];
    if(ht->bDel==2){
      continue;
    }
    for(int i =0; i<head->hashAttempts; i++) {
      int res=lookup(ht, ent,buckets[i]%ht->TableSize);
      if(res==unk){ //unkown if in our not
	continue;
      }
      if(res==in){ //is in
	return 0;
      }
      if(ht->bDel){
	continue;
      }
      entry* expected=NULL;
      if(getBool(ht->InnerTable[res])){
	expected=ht->InnerTable[res];
      }
      int cmp= __atomic_compare_exchange(ht->InnerTable+res,
					 &expected,
					 &ent,
					 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
      if(cmp){
	return 1;
      }
      else{
	if(getPtr(ht->InnerTable[res])->val==ent->val){
	  return 0;
	}
      }
    }
    LocalCur=head->cur;
  }
  SubTable* new_table=createTable(head->TableArray[LocalCur-1]->TableSize<<1);
  addDrop(head, new_table, LocalCur, ent);
  start=LocalCur;
  }
}


int getStart(HashTable* head){
  return head->start;
}

void* delThread(void* targ){
  HashTable* head=(HashTable*)targ;
  int first=1, skip=0;
  SubTable* ht=NULL;
  while(1){
    skip=0;
    if(!first&&!head->stopResize){
      for(int i =0;i<head->cur;i++){
	ht=head->TableArray[i];
	if(ht->tDeleted>(ht->TableSize>>dClear)&&i!=(head->cur-1)&&!ht->bDel){
	  head->delIndex=i;
	  ht->bDel=1;
	  skip=1;
	  break;
	}
      }
    }
    if(!skip){
      pthread_mutex_unlock(&head->fMutex);
      first=0;
      head->deleting=0;
      pthread_cond_wait(&head->dCond, &head->dMutex);
      pthread_mutex_lock(&head->fMutex);
    }
    if(head->stopResize){
      pthread_mutex_unlock(&head->fMutex);
      return NULL;
    }
    ht=head->TableArray[head->delIndex];
    for(int i =0;i<ht->TableSize;i++){
      if(!getBool(ht->InnerTable[i])&&ht->InnerTable[i]){
	insertTable(head, head->delIndex+1, ht->InnerTable[i], 0);
      }
  }
    ht->bDel=2;
    if(head->delIndex==head->start){
      head->start++;
    }
  }
}

HashTable* initTable(HashTable* head, int InitSize, int HashAttempts, int numThreads, unsigned int* seeds, double lines){
  head=(HashTable*)calloc(1,sizeof(HashTable));
  head->seeds=seeds;
  head->hashAttempts=HashAttempts;
  head->TableArray=(SubTable**)calloc(max_tables,sizeof(SubTable*));
  head->TableArray[0]=createTable(InitSize);
  head->cur=1;
  head->start=0;
  head->stopResize=0;
  pthread_mutex_init(&head->fMutex,NULL);
  pthread_create(&head->dThread,NULL, delThread,(void*)head);
  return head;
}

static SubTable* 
createTable(int tsize){
  SubTable* ht=(SubTable*)malloc(sizeof(SubTable));
  ht->TableSize=tsize;
  ht->InnerTable=(entry**)calloc((ht->TableSize),sizeof(entry*));
  ht->bDel=0;
  ht->tDeleted=0;
  return ht;
}
