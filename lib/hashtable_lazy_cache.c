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
const char* tablename = "open/cache/lazymove:V" VERSION;
const char* shortname = "OML:V" VERSION;

//a sub table (this should be hidden)
typedef struct SubTable {
  entry** InnerTable; //rows (table itself)
  int * threadCopy;
  int TableSize; //size
} SubTable;

// head of cache: this is the main hahstable
// head of cache: this is the main hahstable
typedef struct HashTable{
  SubTable** TableArray; //array of tables
  unsigned long start;
  unsigned int * seeds;
  int hashAttempts;
  int readsPerLine;
  int logReadsPerLine;
  int numThreads;
  int cur; //current max index (max exclusive)
} HashTable;


#include "cache-constants.h"
//extern const int lineSize;
//extern const int logLineSize;
//extern const int entPerLine;


#define max_tables 64 //max tables to create

//return values for checking table.  Returned by lookupQuery
#define kSize 4
#define notIn -3 
#define in -1
#define unk -2
const int thrLog = logLineSize-2;
const int thrEnt = lineSize>>2;
#define getInd(X) (X<<thrLog)
#define min(X, Y)  ((X) < (Y) ? (X) : (Y))
#define max(X, Y)  ((X) < (Y) ? (Y) : (X))
#define hashtag(X) genHashTag(X)


inline int getBool(entry* ent){
  return ((unsigned long)ent)&1;
}
inline entry* getPtr(entry* ent){
  unsigned long mask=1;
  return (entry*)(((unsigned long)ent)&(~mask));
}

inline int setPtr(entry** ent){
  entry* newEnt=(entry*)((unsigned long)(*ent)|1);
  entry* exEnt= getPtr(*ent);
  return __atomic_compare_exchange(ent,&exEnt, &newEnt, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

//returns tag from given entry (highest byte)
inline char getEntTag(entry* ent){

  return (char)(((unsigned long)ent)>>56);
}

//returns the ptr for a given entry (0s out the tag bits)
inline entry*  getEntPtr(entry* ent){
  entry* ret= (entry*)((((unsigned long)ent)<<8)>>8);
  return getPtr((entry*)((((unsigned long)ent)<<8)>>8));
}

//sets the tag for a given entry as the highest byte in the address
inline void setTag(entry** entp, char tag){
  unsigned long newPtr=tag;
  newPtr=newPtr<<56;
  newPtr|=(unsigned long)(*entp);
  *entp=(entry*)newPtr;
}

//generates a tag (ands top half and bottum half till 1 byte)
inline char genHashTag(unsigned long key){
  int tag=key^(key>>16);
  tag=tag^(tag>>8);
  return (char)(tag&0xff);
}

int deleteVal(HashTable* head, unsigned long val){
  return 1;
}
int insertTableTag(HashTable* head,  int start, entry* ent, int tid);
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
lookupQuery(SubTable* ht, unsigned long val, unsigned int s, char vTag){

  //get index
  
  //if find null slot know item is not in hashtable as would have been added there otherwise
  if(getEntPtr(ht->InnerTable[s])==NULL){
    return notIn;
  }
    if(vTag!=getEntTag(ht->InnerTable[s])){
    return unk;
  }

  //values equal return index so can access later
  else if(val==getEntPtr(ht->InnerTable[s])->val){
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
int sumArr(int* arr, int size){
  int sum=0;
  for(int i =0;i<size;i+=thrEnt){
    sum+=arr[i];
  }
  return sum;
}

//api function user calls to query the table for a given entry. Returns 1 if found, 0 otherwise.
int checkTableQuery(HashTable* head, unsigned long val){
  SubTable* ht=NULL;
  unsigned int buckets[head->hashAttempts];
  int uBound=head->readsPerLine;
  int ha=head->hashAttempts;
    for(int i =0;i<ha;i++){
      buckets[i]=murmur3_32((const uint8_t *)&val, kSize, head->seeds[i]);
    }
    char tag=hashtag(val);

  //iterate through sub tables
  for(int j=head->start;j<head->cur;j++){
    //    IncrStat(checktable_outer);
    ht=head->TableArray[j];
    //iterate through hash functions
    
    for(int i =0; i<ha; i++) {
      int s=(buckets[i]%(ht->TableSize>>head->logReadsPerLine))<<head->logReadsPerLine;
      
      //call the new line before time amt of computation just cuz...
      __builtin_prefetch(ht->InnerTable[s+(tag&(uBound-1))]);
      //check this line
      for(int c=tag;c<uBound+tag;c++){
	  __builtin_prefetch(ht->InnerTable[s+((c+1)&(uBound-1))]);

      //get results of lookup
      int res=lookupQuery(ht, val,  s+(c&(uBound-1)), tag);
      if(res==unk){ //unkown if in our not
	continue;
      }
      if(res==notIn){
	return 0;		/* indicate not found */
      }
      // otherwise, it is found
      return 1;
    }
    }
  }
  // we never found it
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
    printf("Tables %lu-%d:\n", head->start, head->cur);
  }
  for(int i = 0;i<head->cur; i++){
    ht=head->TableArray[i];
    totalSize+=ht->TableSize;
    for(int j =0;j<ht->TableSize;j++){
      if(getEntPtr(ht->InnerTable[j])!=NULL){
	if(!getBool(ht->InnerTable[j])){
	  //	  free(getEntPtr(ht->InnerTable[j]));
	  count++;
	  if(verbose){
	    items[i]++;
	  }
	}
      }
    }
    if(verbose){
      int sumB=0;
      for(int n =0;n<ht->TableSize;n++){
	sumB+=getBool(ht->InnerTable[n]);
      }
      printf("%d: %d/%d - %d/%d\n", 
	     i, items[i], ht->TableSize, sumArr(ht->threadCopy, head->numThreads), sumB);
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
  //  free(ht->copyBools);
  free(ht);
}


//check if entry for a given hashing vector is in a table. Returns in if entry is already
//in the table, s if the value is not in the table, and unk to try the next hash function
static int lookup(HashTable* head, SubTable* ht, entry* ent, unsigned int s, int doCopy, int tid){


  //if found null slot return index so insert can try and put the entry in the index
  if(getEntPtr(ht->InnerTable[s])==NULL){
    return s;
  }
  
  //found value, return in
    if(getEntTag(ht->InnerTable[s])==getEntTag(ent)){
      if(getEntPtr(ht->InnerTable[s])->val==getEntPtr(ent)->val){
	return in;
      }
    }

  //neither know if value is in or not, first check if this is smallest subtable and 
  //resizing is take place. If so move current item at subtable to next one.
  if(doCopy&&(!getBool(ent))){
    unsigned long exCopy=0;
    unsigned long newCopy=1;
    //    IncrStat(lookup_copy);


    //set the copyBool for the slot to true to indicate the item is being copied
    int res=    setPtr(&ht->InnerTable[s]);

    //succesfully set by this thread
    if(res){
      unsigned long exStart=head->start;
      unsigned long newStart=exStart+1;

      //add item to next sub table
      insertTableTag(head, head->start+1, getEntPtr(ht->InnerTable[s]), tid);

      //increment thread index
      ht->threadCopy[getInd(tid)]++;
      if(ht->TableSize==sumArr(ht->threadCopy, head->numThreads)){
	//if all items have been copied (sum of all threads copy values equals sub table size)
	//update start field in the hashtable (CAS so it cant be double updated).
	__atomic_compare_exchange(&head->start,&exStart, &newStart, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
      }

    }
  }
  //return unk
  return unk;
}

//function to add new subtable to hashtable if dont find open slot 
static int addDrop(HashTable* head, SubTable* toadd, int AddSlot, entry* ent, int tid, int start){
  //  IncrStat(adddrop);

  //try and add new preallocated table (CAS so only one added)
  SubTable* expected=NULL;
  int res = __atomic_compare_exchange(&head->TableArray[AddSlot] ,&expected, &toadd, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
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
    //    IncrStat(addrop_fail);
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

int insertTableTag(HashTable* head,  int start, entry* ent, int tid){
  //create the tag for the entry and set it
  unsigned int buckets[head->hashAttempts];
  char tag = getEntTag(ent);
  //  tag=tag&(head->readsPerLine-1);
  int uBound=head->readsPerLine;
  int ha=head->hashAttempts;
  for(int i =0;i<ha;i++){
    buckets[i]=murmur3_32((const uint8_t *)&getEntPtr(ent)->val, 
			  kSize, 
			  head->seeds[i]);
  }
  SubTable* ht=NULL;
  int LocalCur=head->cur;
  while(1){
    //iterate through subtables
    for(int j=start;j<head->cur;j++){
      ht=head->TableArray[j];

      //do copy if there is a new bigger subtable and currently in smallest subtable
      int doCopy=0;//(j==head->start)&&(head->cur-head->start>1);
      //iterate through hash functions
      for(int i =0; i<ha; i++) {
	int s=(buckets[i]%(ht->TableSize>>head->logReadsPerLine))<<head->logReadsPerLine;

	//call the new line before time amt of computation just cuz...
	__builtin_prefetch(ht->InnerTable[s+(tag&(uBound-1))]);
        //check this line
	for(int c=tag;c<uBound+tag;c++){
	  __builtin_prefetch(ht->InnerTable[s+((c+1)&(uBound-1))]);

	  //lookup value in sub table
	  int res=lookup(head, ht, ent, s+(c&(uBound-1)), doCopy, tid);
	  if(res==unk){ //unkown if in our not
	    continue;
	  }
	  if(res==in){ //is in
	    //	free(ent);
	    return 0;
	  }

	  //      IncrStat(inserttable_inserts);

	  //if return was null (is available slot in sub table) try and add with CAS.
	  //if succeed return 1, else if value it lost to is item itself return. If neither
	  //continue trying to add
	  entry* expected=NULL;
	  int cmp= __atomic_compare_exchange((ht->InnerTable+res),
					     &expected,
					     &ent,
					     1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
	  if(cmp){
	    return 1;
	  }
	  else{
	    if(getEntPtr(ht->InnerTable[res])->val==getEntPtr(ent)->val){
	      return 0;
	    }
	  }
	}
      }
      LocalCur=head->cur;
    }
    

    //if found no available slot in hashtable create new subtable and add it to hashtable
    //then try insertion again
    SubTable* new_table=createTable(head, head->TableArray[LocalCur-1]->TableSize<<1);
    addDrop(head, new_table, LocalCur, ent, tid, start+1);
    start=LocalCur;
  }
}


//insert a new entry into the table. Returns 0 if entry is already present, 1 otherwise.

int insertTable(HashTable* head,  int start, entry* ent, int tid){
  //create the tag for the entry and set it
  unsigned int buckets[head->hashAttempts];
  char tag=hashtag(getEntPtr(ent)->val);
  setTag(&ent, tag);

  //  tag=tag&(head->readsPerLine-1);
  int uBound=head->readsPerLine;
  int ha=head->hashAttempts;
  for(int i =0;i<ha;i++){
    buckets[i]=murmur3_32((const uint8_t *)&getEntPtr(ent)->val, 
			  kSize, 
			  head->seeds[i]);
  }
  SubTable* ht=NULL;
  int LocalCur=head->cur;
  while(1){
    //iterate through subtables
    for(int j=start;j<head->cur;j++){
      ht=head->TableArray[j];

      //do copy if there is a new bigger subtable and currently in smallest subtable
      int doCopy=(j==head->start)&&(head->cur-head->start>1);
      //iterate through hash functions
      for(int i =0; i<ha; i++) {
	int s=(buckets[i]%(ht->TableSize>>head->logReadsPerLine))<<head->logReadsPerLine;

	//call the new line before time amt of computation just cuz...
	__builtin_prefetch(ht->InnerTable[s+(tag&(uBound-1))]);
        //check this line
	for(int c=tag;c<uBound+tag;c++){
	  __builtin_prefetch(ht->InnerTable[s+((c+1)&(uBound-1))]);

	  //lookup value in sub table
	  int res=lookup(head, ht, ent, s+(c&(uBound-1)), doCopy, tid);
	  if(res==unk){ //unkown if in our not
	    continue;
	  }
	  if(res==in){ //is in
	    //	free(ent);
	    return 0;
	  }

	  //      IncrStat(inserttable_inserts);

	  //if return was null (is available slot in sub table) try and add with CAS.
	  //if succeed return 1, else if value it lost to is item itself return. If neither
	  //continue trying to add
	  entry* expected=NULL;
	  int cmp= __atomic_compare_exchange((ht->InnerTable+res),
					     &expected,
					     &ent,
					     1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
	  if(cmp){
	    return 1;
	  }
	  else{
	    if(getEntPtr(ht->InnerTable[res])->val==getEntPtr(ent)->val){
	      return 0;
	    }
	  }
	}
      }
      LocalCur=head->cur;
    }
    

    //if found no available slot in hashtable create new subtable and add it to hashtable
    //then try insertion again
    SubTable* new_table=createTable(head, head->TableArray[LocalCur-1]->TableSize<<1);
    addDrop(head, new_table, LocalCur, ent, tid, start+1);
    start=LocalCur;
  }
}


int logofint(int input){
  int a=0;
  while(input>>a){
    a++;
  }
  return a-1;
}


//initial hashtable. First table head will be null, after that will just reinitialize first table
//returns a pointer to the hashtable
HashTable* initTable(HashTable* head, int InitSize, int HashAttempts, int numThreads, unsigned int* seeds, double lines){

  head=(HashTable*)calloc(1,sizeof(HashTable));
  head->seeds=seeds;
  if(InitSize<max(((int)((lines)*entPerLine)),entPerLine<<1)){
    printf("Changing value for initSize from %d to %d\n", 
	   InitSize, max(((int)((lines)*entPerLine)), entPerLine<<1));
    InitSize=max(((int)((lines)*entPerLine)),entPerLine<<1);
  }
  head->readsPerLine=(int)(entPerLine*lines);
  head->logReadsPerLine=logofint(head->readsPerLine);
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
  ht->threadCopy=( int*)aligned_alloc(lineSize,head->numThreads<<logLineSize);
  memset(ht->threadCopy, 0, head->numThreads<<logLineSize);
  return ht;
}
