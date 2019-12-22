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
#include <assert.h>
#include "cache-params.h"
#include "hashtable.h"
#include "hash.h"
extern __thread int threadId;		/* internal thread id */
#include "hashstat.h"

#define VERSION "0.1"

//used for determine size of next subtable
#define DOUBLE_NEXT 1.1 //ratio of insert/delete to double next subtable size
#define TO_MANY_TABLES 8 //diff between max/min table to double next subtable size


const char* tablename = "open/multiprobe/lazymove:V" VERSION;
const char* shortname = "OML:V" VERSION;

//a sub table (this should be hidden)
typedef struct SubTable {
  entry** InnerTable; //rows (table itself)

  //for keeping track of when all items from min sub table have been moved
  int * threadCopy;

  //this could be estimated by using only insert count
  //and just insertCount/total size but if its 100% inserts
  //we want to double next subtable size so we need delete estimate too
  int * insertCount;
  int * deleteCount;
  
  int TableSize; //size
} SubTable;


// head of cache: this is the main hahstable
typedef struct HashTable{
  SubTable** TableArray; //array of tables
  unsigned int * seeds;
  int hashAttempts;
  unsigned long start;
  int cur; //current max index (max exclusive)
  int numThreads;
} HashTable;

#include "cache-constants.h"
#define max_tables 64 //max tables to create

//return values for checking table.  Returned by lookupQuery
#define dUnk -4 //deleted same value
#define notIn -3 //item is not in (used in query/delete)
#define in -1 //item is already in
#define unk -2 //unkown keep looking

#define kSize 4 //sizeof key for hashing
#define copy 0x1 //sets 1s bit of ent ptr for resizing

#define min(X, Y)  ((X) < (Y) ? (X) : (Y))

inline int getBool(entry* ent){
  return ((unsigned long)ent)&copy;
}

inline entry* getPtr(entry* ent){
  unsigned long mask=copy;
  return (entry*)(((unsigned long)ent)&(~mask));
}

inline int setPtr(entry** ent){
  entry* newEnt=(entry*)((unsigned long)(*ent)|copy);
  entry* exEnt= (entry*)(((unsigned long)getPtr(*ent)));
  return __atomic_compare_exchange(ent,&exEnt, &newEnt, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

inline int isDeleted(entry* ptr){
  return ptr->isDeleted;
}
inline int setDelete(entry* ptr){
  if(!isDeleted(ptr)){
    unsigned long expec=0;
    unsigned long newV=1;
    return __atomic_compare_exchange(&ptr->isDeleted,&expec, &newV, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
  }
  return 0;
}

inline int unDelete(entry* ptr){
  if(isDeleted(ptr)){
    unsigned long expec=1;
    unsigned long newV=0;
    return __atomic_compare_exchange(&ptr->isDeleted,&expec, &newV, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
  }
  return 0;
}

int sumArr(  int* arr, int size);

static int 
lookupQuery(SubTable* ht, unsigned long val, unsigned int s);

//deleting a value just works be finding it and if it exists setting the deleted
//unsigned long (could be as small as char) to 1
int deleteVal(HashTable* head, unsigned long val, int tid){
  SubTable* ht=NULL;
  unsigned int buckets[head->hashAttempts];
  for(int i =0;i<head->hashAttempts;i++){
    buckets[i]=murmur3_32((const uint8_t *)&val, kSize, head->seeds[i]);
  }
  //subtable array is now circular so modding by max_tables
  for(int iter_j=head->start;iter_j<head->cur;iter_j++){
    int j = iter_j&(max_tables-1);
    
    ht=head->TableArray[j];
    for(int i =0; i<head->hashAttempts; i++) {

      int res=lookupQuery(ht, val, buckets[i]%ht->TableSize);
      if(res==unk){ //unkown if in our not (space was wrong value)
	continue;
      }
      if(res==notIn){ //not in (space was null)
	return 0;
      }

      int ret=setDelete(getPtr(ht->InnerTable[res]));

      //increment delete count for the table, ret is return from CAS of whether this
      //thread was one that succesfully deleted
      ht->deleteCount[tid]+=ret;
      return ret; 
    }
  }
}



// create a sub table
static SubTable* createTable(HashTable* head, int hsize);
// free a subtable 
static void freeTable(SubTable* table);

//creates new hashtable in the tablearray
static int addDrop(HashTable* head, SubTable* toadd, int AddSlot, entry* ent, int tid, int start);

//lookup function in insertTrial to check a given inner table
static int lookup(HashTable* head, SubTable* ht, entry* ent,unsigned int s, int doCopy, int tid);

//returns whether an entry is found in a given subtable/overall hashtable. notIn means not
//in the hashtable, s means in the hashtable and was found (return s so can get the value
//of the item). unk means unknown if in table.
static int 
lookupQuery(SubTable* ht, unsigned long val, unsigned int s){

  //if find null slot know item is not in hashtable as would have been added there otherwise
  if(getPtr(ht->InnerTable[s])==NULL){
    return notIn;
  }

  //values equal return index so can access later
  else if(val==getPtr(ht->InnerTable[s])->val){
    if(isDeleted(getPtr(ht->InnerTable[s]))){
      //if deleted dont want query to return true
      return unk;
    }
    return s;
  }

  //go another value, try next hash function
  return unk;
}


//helper function for accessing start in harness.c
int getStart(HashTable* head){
  return (int)head->start;
}



//helper function that returns sum of array up to size
int sumArr(  int* arr, int size){
  int sum=0;
  for(int i =0;i<size;i++){
    sum+=arr[i];
  }
  return sum;
}

//get ratio of total inserted items/total deleted items from a given subtable
//used to determine the size of next subtable to be created
static double
getDelRatio(HashTable* head, SubTable* subt){
  int totalDelete = sumArr(subt->deleteCount, head->numThreads);
  int totalInsert = sumArr(subt->insertCount, head->numThreads);
  return ((double)totalInsert) / ((double)totalDelete);
}


//api function user calls to query the table for a given entry. Returns 1 if found, 0 otherwise.
int checkTableQuery(HashTable* head, unsigned long val){
  SubTable* ht=NULL;

  //iterate through sub tables
  unsigned int buckets[head->hashAttempts];
  for(int i =0;i<head->hashAttempts;i++){
    buckets[i]=murmur3_32((const uint8_t *)&val, kSize, head->seeds[i]);
  }
  for(int iter_j=head->start;iter_j<head->cur;iter_j++){
    int j = iter_j&(max_tables-1);
    ht=head->TableArray[j];
    for(int i =0; i<head->hashAttempts; i++) {
      int res=lookupQuery(ht, val, buckets[i]%ht->TableSize);
      if(res==unk){ //unkown if in our not
	continue;
      }
      if(res==notIn){
	return 0;		/* indicate not found */
      }
      return 1;
    }
  }
  //was never found
  return 0;
}

//free all memory for table, last will free hashtable head as well.
//verbose will print extra information.
double freeAll(HashTable* head, int last, int verbose){
  SubTable* ht=NULL;
  double count=0;
  double totalSize=0;
  int * items=NULL;
  if(verbose){
    items=(int*)calloc(sizeof(int), head->cur);
    printf("Tables %lu-%d:\n",head->start, head->cur);
  }
  for(int iter_i=head->start;iter_i<head->cur;iter_i++){
    int i = iter_i&(max_tables-1);
    ht=head->TableArray[i];
    totalSize+=ht->TableSize;
    for(int j =0;j<ht->TableSize;j++){
      if(getPtr(ht->InnerTable[j])!=NULL){
	if(!getBool(ht->InnerTable[j])&&(!isDeleted(getPtr(ht->InnerTable[j])))){
	  //	  free(getPtr(ht->InnerTable[j]));
	  count++;
	  if(verbose){
	    items[i]++;
	  }
	}
      }
    }
    if(verbose){
      int sumD=0;
      for(int n =0;n<ht->TableSize;n++){
	sumD+=!getBool(ht->InnerTable[n]);
      }
      printf("%d: %d/%d - %d/%d - %d/%d=%.3lf\n", 
	     i, items[i], ht->TableSize,
	     sumArr(ht->threadCopy, head->numThreads),
	     sumD,
	     sumArr(ht->insertCount, head->numThreads),
	     sumArr(ht->deleteCount, head->numThreads),
	     getDelRatio(head, ht));
    }
    free(ht->InnerTable);
    free(ht->threadCopy);
    //    free(ht->copyBools);
    free(ht);
  }
  
  free(head->TableArray);

  if(verbose){
    free(items);
    printf("Total: %d\n", (int)count);
  }

  free(head);
  return count/totalSize;  
}

//frees a given table that was created for adddrop (that failed)
static void 
freeTable(SubTable* ht){
  free(ht->InnerTable);
  free(ht->threadCopy);
  free(ht->insertCount);
  free(ht->deleteCount);
  free(ht);
}

//check if entry for a given hashing vector is in a table. Returns in if entry is already
//in the table, s if the value is not in the table, and unk to try the next hash function
static int lookup(HashTable* head, SubTable* ht, entry* ent, unsigned int s, int doCopy, int tid){
  
  int res;
  //if found null slot return index so insert can try and put the entry in the index
  if(getPtr(ht->InnerTable[s])==NULL){
    return s;
  }

  //if values equal case  
  else if(getPtr(ht->InnerTable[s])->val==getPtr(ent)->val){

    if(isDeleted(getPtr(ht->InnerTable[s]))){
      //if item is deleted and copy mode (we in smallest subtable)
      //2 cases.
      if(doCopy){
	if(getBool(ht->InnerTable[s])){
	  //case 1 another thread has already marked this slot as having been moved
	  //so continue searching
	  return unk;
	}
	if(setPtr(&ht->InnerTable[s])){
	  //case 2 we want to mark this slot. Better not to undelete here because
	  //this subtable will soon be removed and will cause another re-insert
	  goto gotBool;
	}
	return unk;
      }
      //if not min subtable return dUnk (for undelete)
      return dUnk;
    }
    //not deleted, return in
    return in;
  }

  //neither know if value is in or not, first check if this is smallest subtable and 
  //resizing is take place. If so move current item at subtable to next one.
  if(doCopy){
    res=setPtr(&ht->InnerTable[s]);
    //succesfully set by this thread
    if(res){
    gotBool:;

      //exstart/newstart for CAS that might take place
      unsigned long exStart=head->start;
      unsigned long newStart=exStart+1;

      //if item is not deleted copy up (this is how deleted items are culled as
      //they will not be copied)
      if(!isDeleted(getPtr(ht->InnerTable[s]))){
	//add item to next sub table
	insertTable(head, head->start+1, getPtr(ht->InnerTable[s]), tid);
	
      }
      //increment thread index
      ht->threadCopy[tid]++;

      //if all slots have been copied increment min subtable
      if(ht->TableSize==sumArr(ht->threadCopy, head->numThreads)){
	if(__atomic_compare_exchange(&head->start,&exStart, &newStart, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED)){

	  //this is a memory leak/definition of magic number. I really do NOT want to use
	  //a reference counter as they are incredibly slow. Going back a few subtables should
	  //gurantee no other thread is in it. Once we have a good way to do this we can
	  //actually include the freeing portion.
	  head->TableArray[(exStart-(3))&(max_tables-1)] = NULL;
	}
      }
    }
  }
  //return unk
  return unk;
}

//function to add new subtable to hashtable if dont find open slot 
static int addDrop(HashTable* head, SubTable* toadd, int AddSlot, entry* ent, int tid, int start){

  //make the array circular so add/delete continuous will always have space. Assumed that
  //resizer will keep up with insertion rate (this is probably provable as when resize is active
  //(which occurs when num subtables > threshhold each insert brings an old item with it). Also
  //if the diff between max/min subtables exceed a certain bound will start doubling again
  //irrelivant of delete/insert ratio

  //try and add new preallocated table (CAS so only one added)
  SubTable* expected=NULL;
  int res = __atomic_compare_exchange(&head->TableArray[AddSlot&(max_tables-1)] ,&expected, &toadd, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
  if(res){
    //if succeeded try and update new max then insert item
    int newSize=AddSlot+1;
    __atomic_compare_exchange(&head->cur,
			      &AddSlot,
			      &newSize,
			      1,__ATOMIC_RELAXED, __ATOMIC_RELAXED);
  }
  else{
    //if failed free subtable then try and update new max then insert item
    freeTable(toadd);
    int newSize=AddSlot+1;
    __atomic_compare_exchange(&head->cur,
			      &AddSlot,
			      &newSize,
			      1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);

  }
  return 0;
}


//insert a new entry into the table. Returns 0 if entry is already present, 1 otherwise.
int insertTable(HashTable* head,  int start, entry* ent, int tid){

  SubTable* ht=NULL;

  //use local max for adddroping subtables (otherwise race condition where 2 go to add
  //to slot n, one completes addition and increments head->cur and then second one goes
  //and double adds. Won't affect correctness but best not have that happen.
  int LocalCur=head->cur;
  
  unsigned int buckets[head->hashAttempts];
  for(int i =0;i<head->hashAttempts;i++){
    buckets[i]=murmur3_32((const uint8_t *)&(getPtr(ent)->val), kSize, head->seeds[i]);
  }
  while(1){
    //iterate through subtables
    //again is mod max_subtables
    for(int iter_j=start;iter_j<head->cur;iter_j++){
      int j = iter_j&(max_tables-1);
      ht=head->TableArray[j];

      //do copy if there is a new bigger subtable and currently in smallest subtable
      int doCopy=(j==(head->start&(max_tables-1)))&&(head->cur-head->start>1);

      //iterate through hash functions
      for(int i =0; i<head->hashAttempts; i++) {

	//lookup value in sub table
	int res=lookup(head, ht, ent, buckets[i]%ht->TableSize, doCopy, tid);
	if(res==unk){ //unkown if in our not
	  continue;
	}
	if(res==in){ //is in
	  return 0;
	}



	//if res == dUnk we want to undelete
	if(res==dUnk){
	  res=buckets[i]%ht->TableSize; //get slot

	  //this is a CAS
	  int ret =unDelete(getPtr(ht->InnerTable[res]));
	  ht->insertCount[tid]+=ret;
	  return ret;
	}
	else{
	  
	  //if return was null (is available slot in sub table) try and add with CAS.
	  //if succeed return 1, else if value it lost to is item itself return. If neither
	  //continue trying to add
	  entry* expected=NULL;

	  int cmp= __atomic_compare_exchange((ht->InnerTable+res),
					     &expected,
					     &ent,
					     1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
	  if(cmp){
	    //if we win CAS increment insert count and return 1
	    ht->insertCount[tid]++;
	    return 1;
	  }
	  else{
	    //else check if value that was lost to is same, if not keep going, if is
	    //turn 0
	      if(getPtr(ht->InnerTable[res])->val==getPtr(ent)->val){
		return 0;
	    }
	  }
	}
      }
      //update locacur
      LocalCur=head->cur;
    }

    //if found no available slot in hashtable create new subtable and add it to hashtable
    //then try insertion again

    //next table size defaults to same size
    int nextTableSize = head->TableArray[(LocalCur-1)&(max_tables-1)]->TableSize;
    double deleteRatio = getDelRatio(head, head->TableArray[(LocalCur-1)&(max_tables-1)]);

    //if totalinserts/totaldeletes > threshhold or subtables are growing to fast
    //we want to double next.
    //first part ensures not constant growth if unnecissary (i.e insert a bunch, delete it all,
    //re insert, redelete, etc...) and second part ensures that subtable size will reach
    //ideal state. These are both heuristics that should be tuned.
    if(deleteRatio>DOUBLE_NEXT || head->cur-head->start>TO_MANY_TABLES){
      nextTableSize = nextTableSize<<1;
    }

    //create next subtables
    SubTable* new_table=createTable(head, nextTableSize);
    addDrop(head, new_table, LocalCur, ent, tid, start+1);
    start=LocalCur;
  }
}


//initial hashtable. First table head will be null, after that will just reinitialize first table
//returns a pointer to the hashtable
HashTable* initTable(HashTable* head, int InitSize, int HashAttempts, int numThreads, unsigned int* seeds, double lines){

  head=(HashTable*)calloc(1,sizeof(HashTable));
  head->seeds=seeds;
  head->hashAttempts=HashAttempts;
  head->numThreads=numThreads;
  head->TableArray=(SubTable**)calloc(max_tables,sizeof(SubTable*));
  head->TableArray[0]=createTable(head, InitSize);
  head->cur=1;
  head->start=0;
  return head;
}



//creates a subtable 
static SubTable* 
createTable(HashTable* head, int tsize){
  //  IncrStat(createtable);
  SubTable* ht=(SubTable*)calloc(1,sizeof(SubTable));
  
  ht->TableSize=tsize;
  ht->InnerTable=(entry**)calloc((ht->TableSize),sizeof(entry*));
  ht->threadCopy=( int*)calloc(head->numThreads,sizeof(int));
  ht->insertCount=( int*)calloc(head->numThreads,sizeof(int));
  ht->deleteCount=( int*)calloc(head->numThreads,sizeof(int));
  return ht;
}
