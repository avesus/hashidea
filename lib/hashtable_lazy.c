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
const char* tablename = "open table, multiple probes, lazy move:version " VERSION;

//a sub table (this should be hidden)
typedef struct SubTable {
  entry** InnerTable; //rows (table itself)
  unsigned long* copyBools;
  int * threadCopy;
  int TableSize; //size
} SubTable;

// head of cache: this is the main hahstable
typedef struct HashTable{
  SubTable** TableArray; //array of tables
  unsigned int * seeds;
  int hashAttempts;
  int start;
  int cur; //current max index (max exclusive)
  int numThreads;
} HashTable;


#define max_tables 64 //max tables to create

//return values for checking table.  Returned by lookupQuery
#define notIn 0 
#define in -1
#define unk -2

// create a sub table
static SubTable* createTable(HashTable* head, int hsize);
// free a subtable 
static void freeTable(SubTable* table);

//creates new hashtable in the tablearray
static int addDrop(HashTable* head, SubTable* toadd, int AddSlot, entry* ent, int tid);

//lookup function in insertTrial to check a given inner table
static int lookup(HashTable* head, SubTable* ht, entry* ent, int seedIndex, int doCopy, int tid);


static int 
lookupQuery(SubTable* ht, entry* ent, unsigned int seed){
  unsigned int s=murmur3_32((const uint8_t *)&ent->val, sizeof(ent->val), seed)%ht->TableSize;
  if(ht->InnerTable[s]==NULL){
    return notIn;
  }
  else if(ent->val==ht->InnerTable[s]->val){
    return s;
  }
  return unk;
}

int getStart(HashTable* head){
  return head->start;
}

int sumArr(int* arr, int size){
  int sum=0;
  for(int i =0;i<size;i++){
    sum+=arr[i];
  }
  return sum;
}


int checkTableQuery(HashTable* head, entry* ent){
  SubTable* ht=NULL;
  for(int j=0;j<head->cur;j++){
    ht=head->TableArray[j];
    for(int i =0;i<head->hashAttempts;i++){
      int res=lookupQuery(ht, ent, head->seeds[i]);
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
    totalSize+=ht->TableSize;
    for(int j =0;j<ht->TableSize;j++){
      if(ht->InnerTable[j]!=NULL){
	if(!ht->copyBools[j]){
	free(ht->InnerTable[j]);
	count++;
	if(verbose){
	  items[i]++;
	}
	}
      }
    }
    if(verbose){
      printf("%d/%d\n", items[i], ht->TableSize);
    }
    free(ht->InnerTable);
    free(ht->threadCopy);
    free(ht->copyBools);
    free(ht);
  }
  
  free(head->TableArray);
  if(verbose){
    free(items);
    printf("Total: %d\n", (int)count);
  }
  if(last){
    free(head->seeds);
    free(head);
  }
  return count/totalSize;  
}

static void 
freeTable(SubTable* ht){
  free(ht);
}


//check if entry for a given hashing vector is in a table
static int lookup(HashTable* head, SubTable* ht, entry* ent, int seedIndex, int doCopy, int tid){

  unsigned int s= murmur3_32((const uint8_t *)&ent->val, sizeof(ent->val), head->seeds[seedIndex])%ht->TableSize;
  entry* temp=ht->InnerTable[s];
  if(temp==NULL){
    return s;
  }
  else if(temp->val==ent->val){
    return in;
  }
  if(doCopy&&(!ht->copyBools[s])){
    unsigned long exCopy=0;
    unsigned long newCopy=1;
    int res = __atomic_compare_exchange(&ht->copyBools[s],&exCopy, &newCopy, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
    if(res){
      insertTable(head, head->start+1, temp, tid);
      ht->threadCopy[tid]++;
      if(ht->TableSize==sumArr(ht->threadCopy, head->numThreads)){
	head->start++;
      
      }
    }
  }
  return unk;
}



static int addDrop(HashTable* head, SubTable* toadd, int AddSlot, entry* ent, int tid){
  SubTable* expected=NULL;
  int res = __atomic_compare_exchange(&head->TableArray[AddSlot] ,&expected, &toadd, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
  if(res){
    int newSize=AddSlot+1;
    __atomic_compare_exchange(&head->cur,
			      &AddSlot,
			      &newSize,
			      1,__ATOMIC_RELAXED, __ATOMIC_RELAXED);
    insertTable(head, head->start+1, ent, tid);
  }
  else{
    freeTable(toadd);
    int newSize=AddSlot+1;
    __atomic_compare_exchange(&head->cur,
			      &AddSlot,
			      &newSize,
			      1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);

    insertTable(head, head->start+1, ent, tid);
  }
  return 0;
}



int insertTable(HashTable* head,  int start, entry* ent, int tid){

  SubTable* ht=NULL;
  int LocalCur=head->cur;
  for(int j=start;j<head->cur;j++){
    ht=head->TableArray[j];
    int doCopy=(j==head->start)&&(head->cur-head->start>1);
    for(int i =0;i<head->hashAttempts;i++){
      int res=lookup(head, ht, ent, i, doCopy, tid);
      if(res==unk){ //unkown if in our not
	continue;
      }
      if(res==in){ //is in
	return 0;
      }

      entry* expected=NULL;
      int cmp= __atomic_compare_exchange(&ht->InnerTable[res],
					 &expected,
					 &ent,
					 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
      if(cmp){
	return 1;
      }
      else{
	if(ht->InnerTable[res]->val==ent->val){
	  return 0;
	}
      }
    }
    LocalCur=head->cur;
  }
  SubTable* new_table=createTable(head, head->TableArray[LocalCur-1]->TableSize<<1);
  addDrop(head, new_table, LocalCur, ent, tid);
}


static unsigned int*
initSeeds(int HashAttempts){
  unsigned int * seeds=(unsigned int*)malloc(sizeof(unsigned int)*HashAttempts);
  for(int i =0;i<HashAttempts;i++){
    seeds[i]=random();
  }
  return seeds;
}


HashTable* initTable(HashTable* head, int InitSize, int HashAttempts, int numThreads){
  if(!head){
    head=(HashTable*)calloc(1,sizeof(HashTable));
    head->seeds=initSeeds(HashAttempts);
    head->hashAttempts=HashAttempts;
  }
  head->TableArray=(SubTable**)calloc(max_tables,sizeof(SubTable*));
  head->TableArray[0]=createTable(head, InitSize);
  head->cur=1;
  head->start=0;
  head->numThreads=numThreads;
  return head;
}

static SubTable* 
createTable(HashTable* head, int tsize){
  SubTable* ht=(SubTable*)calloc(1,sizeof(SubTable));
  ht->TableSize=tsize;
  ht->InnerTable=(entry**)calloc(sizeof(entry*),(ht->TableSize));
  ht->threadCopy=(int*)calloc(sizeof(int), head->numThreads);
  ht->copyBools=(unsigned long*)calloc(sizeof(unsigned long),ht->TableSize);
  return ht;
}
