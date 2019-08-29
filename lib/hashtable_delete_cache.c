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
  int TableSize; //size
} SubTable;


typedef struct HashTable{
  SubTable** TableArray; //array of tables
  unsigned int * seeds;
  int hashAttempts;
  int readsPerLine;
  int logReadsPerLine;
  int cur; //current max index (max exclusive)
} HashTable;

extern const int lineSize;
extern const int logLineSize;
extern const int entPerLine;

#define max_tables 64 //max tables to create

//return values for checking table.  Returned by lookupQuery
#define kSize 4
#define notIn -3 
#define in -1
#define unk -2
#define deleted 1

#define min(X, Y)  ((X) < (Y) ? (X) : (Y))
#define max(X, Y)  ((X) < (Y) ? (Y) : (X))
#define hashtag(X) genHashTag(X)
// create a sub table
static SubTable* createTable(int hsize);
// free a subtable 
static void freeTable(SubTable* table);

//creates new hashtable in the tablearray
static int addDrop(HashTable* head, SubTable* toadd, int AddSlot, entry* ent);

//lookup function in insertTrial to check a given inner table
static int lookup(SubTable* ht, entry* ent, unsigned int s);

inline int getBool(entry* ptr){
  return ((unsigned long)ptr)&deleted;
}

//gets pointer by anding off the first 4 bits from a ptr
inline entry* getPtr(entry* ptr){
  unsigned long mask=deleted;
  return (entry*)((((unsigned long)ptr)&(~mask)));
}


//set count value for a pointer
int setPtr(entry** ptr){
  entry* newEnt=(entry*)((unsigned long)(*ptr)|deleted);
  entry* exEnt= getPtr(*ptr);
  return __atomic_compare_exchange(ptr,&exEnt, &newEnt, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
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




static inline int 
lookupQuery(SubTable* ht, unsigned long val, unsigned int s, char vTag){

  if(ht->InnerTable[s]==NULL){
    return notIn;
  }
  if(vTag!=getEntTag(ht->InnerTable[s])){
    return unk;
  }
  else if(val==getEntPtr(ht->InnerTable[s])->val){
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
    int uBound=head->readsPerLine;
      int ha=head->hashAttempts;
      for(int i =0;i<ha;i++){
      buckets[i]=murmur3_32((const uint8_t *)&val, kSize, head->seeds[i]);
    }
    char tag=hashtag(val);
        for(int j=0;j<head->cur;j++){

    ht=head->TableArray[j];
    for(int i =0; i<ha; i++) {
      int s=(buckets[i]%(ht->TableSize>>head->logReadsPerLine))<<head->logReadsPerLine;
      __builtin_prefetch(ht->InnerTable[s+(tag&(uBound-1))]);
      for(int c=tag;c<uBound+tag;c++){
	__builtin_prefetch(ht->InnerTable[s+((c+1)&(uBound-1))]);
	int res=lookupQuery(ht, val, s+(c&(uBound-1)), tag);
      if(res==unk){ //unkown if in our not
	continue;
      }
      if(res==notIn){ //is in
	//	printf("Through this\n");
	return 0;
      }
      //not in (we got null so it would have been there)
      return 1;
    }
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
      if(ht->InnerTable[j]!=NULL&&!getBool(ht->InnerTable[j])){
	//free(ht->InnerTable[j]);
	count++;
	if(verbose){
	  items[i]++;
	}
      }
    }
    if(verbose){
      printf("%d/%d\n", items[i], ht->TableSize);
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
static inline int lookup(SubTable* ht, entry* ent, unsigned int s){

  if(ht->InnerTable[s]==NULL){
    return s;
  }
  if(getEntTag(ht->InnerTable[s])!=getEntTag(ent)){
    return unk;
  }
  else if(getEntPtr(ht->InnerTable[s])->val==getEntPtr(ent)->val){
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
    int uBound=head->readsPerLine;
  int ha=head->hashAttempts;
    for(int i =0;i<ha;i++){
      buckets[i]=murmur3_32((const uint8_t *)&val, kSize, head->seeds[i]);
    }
    char tag=hashtag(val);
  for(int j=0;j<head->cur;j++){

    ht=head->TableArray[j];
    for(int i =0; i<ha; i++) {
            int s=(buckets[i]%(ht->TableSize>>head->logReadsPerLine))<<head->logReadsPerLine;
      
      //call the new line before time amt of computation just cuz...
      __builtin_prefetch(ht->InnerTable[s+(tag&(uBound-1))]);
      //check this line
      for(int c=tag;c<uBound+tag;c++){
	__builtin_prefetch(ht->InnerTable[s+((c+1)&(uBound-1))]);
	int res=lookupQuery(ht, val, s+(c&(uBound-1)), tag);
      if(res==unk){ //unkown if in our not
	continue;
      }
      if(res==notIn){ //is in
	return 0;
      }

      
      return setPtr(&ht->InnerTable[res]);
    }
  }
  }
  return 0;
}


int insertTable(HashTable* head,  int start, entry* ent, int tid){

  unsigned int buckets[head->hashAttempts];
  char tag=hashtag(ent->val);
  setTag(&ent, tag);
  //  int cc=tag;


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
    for(int j=start;j<head->cur;j++){
      ht=head->TableArray[j];
      for(int i =0; i<ha; i++) {
	int s=(buckets[i]%(ht->TableSize>>head->logReadsPerLine))<<head->logReadsPerLine;
	__builtin_prefetch(ht->InnerTable[s+(tag&(uBound-1))]);
	for(int c=tag;c<uBound+tag;c++){
	  __builtin_prefetch(ht->InnerTable[s+((c+1)&(uBound-1))]);
	  int res=lookup(ht, ent,s+(c&(uBound-1)));
	  if(res==unk){ //unkown if in our not
	    continue;
	  }
	  if(res==in){ //is in
	    return 0;
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
	    if(getEntPtr(ht->InnerTable[res])->val==getEntPtr(ent)->val){
	      return 0;
	    }
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
    return 0;
  }
int logofint(int input){
  int a=0;
  while(input>>a){
    a++;
  }
  return a-1;
}
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
  head->TableArray=(SubTable**)calloc(max_tables,sizeof(SubTable*));
  head->TableArray[0]=createTable(InitSize);
  head->cur=1;
  printf("%d, %d, %d, %d, %lf\n", 
	 lineSize, logLineSize, entPerLine, head->readsPerLine, lines);
  return head;
  }

  static SubTable* 
    createTable(int tsize){
    SubTable* ht=(SubTable*)malloc(sizeof(SubTable));
    ht->TableSize=tsize;
    ht->InnerTable=(entry**)calloc((ht->TableSize),sizeof(entry*));
    return ht;
  }
