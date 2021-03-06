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
  volatile unsigned long deleting;
  volatile int stopResize;
  pthread_mutex_t fMutex;
  pthread_mutex_t dMutex;
  pthread_t dThread;
  SubTable** TableArray; //array of tables
  unsigned int * seeds;
  int readsPerLine;
  int logReadsPerLine;
  int delIndex;
  int start;
  int hashAttempts;
  int cur; //current max index (max exclusive)
  
} HashTable;


#include "cache-constants.h"


#define max_tables 64 //max tables to create
#define copy 0x1
#define dClear 2
//return values for checking table.  Returned by lookupQuery
#define kSize 4
#define dUnk -4
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



//returns tag from given entry (highest byte)
inline short getEntTag(entry* ent){

  return (short)(((unsigned long)ent)>>48);
}

//returns the ptr for a given entry (0s out the tag bits)
inline entry*  getEntPtr(entry* ent){
  entry* ret= (entry*)((((unsigned long)ent)<<16)>>16);
  return getPtr((entry*)((((unsigned long)ent)<<16)>>16));
}

//sets the tag for a given entry as the highest byte in the address
inline void setTag(entry** entp, short tag){
  unsigned long newPtr=tag;
  newPtr=newPtr<<48;
  newPtr|=(unsigned long)(*entp);
  *entp=(entry*)newPtr;
}

//generates a tag (ands top half and bottum half till 1 byte)
inline short genHashTag(unsigned long key){
  int tag=key^(key>>16);
  //  tag=tag^(tag>>8);
  return (short)(tag&0xff);
}



static int 
lookupQuery(SubTable* ht, unsigned long val, unsigned int s, short vTag){
  entry* temp=ht->InnerTable[s];
  if(temp==NULL){
    return notIn;
  }
  if(getBool(temp)){
  if(getPtr(temp)==NULL){
    return notIn;
  }
  }

  if(vTag!=getEntTag(temp)){
    return unk;
  }
  
  else if(val==getEntPtr(temp)->val){
    if(isDeleted(getEntPtr(temp))){
      return notIn;
    }
    return s;
  }
  return unk;
}

int checkTableQuery(HashTable* head, unsigned long val){
  SubTable* ht=NULL;
  unsigned int buckets[head->hashAttempts];
    int logReadsPerLine=head->logReadsPerLine;
  int uBound=head->readsPerLine;
  int ha=head->hashAttempts;
  for(int i =0;i<ha;i++){
    buckets[i]=murmur3_32((const uint8_t *)&val, kSize, head->seeds[i]);
  }
  short tag=hashtag(val);

  for(int j=head->start;j<head->cur;j++){

    ht=head->TableArray[j];
    if(ht->bDel==2){
      continue;
    }
    for(int i =0; i<ha; i++) {
      int s=(buckets[i]%(ht->TableSize>>logReadsPerLine))<<logReadsPerLine;
      
      //call the new line before time amt of computation just cuz...
      //      __builtin_prefetch(ht->InnerTable[s+(tag&(uBound-1))]);
      //check this line
      for(int c=tag;c<uBound+tag;c++){
	//	__builtin_prefetch(ht->InnerTable[s+((c+1)&(uBound-1))]);

	//get results of lookup
	int res=lookupQuery(ht, val, s+(c&(uBound-1)), tag);
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
  }
  return 0;
}


double freeAll(HashTable* head, int last, int verbose){
  head->stopResize=1;
  pthread_mutex_unlock(&head->fMutex);
    pthread_join(head->dThread,NULL);
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
      if(ht->InnerTable[j]!=NULL&&(!isDeleted(getEntPtr(ht->InnerTable[j])))){
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
  entry* temp=ht->InnerTable[s];
  if(temp==NULL){
    return s;
  }
  if(getBool(temp)){
  if(getPtr(temp)==NULL){
    return unk;
  }
  }
  if(getEntTag(temp)!=getEntTag(ent)){
    return unk;
  }
  
  else if(getEntPtr(temp)->val==getEntPtr(ent)->val){
    if(isDeleted(getEntPtr(temp))){
      return dUnk;
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




int deleteVal(HashTable* head, unsigned long val, int tid){
  SubTable* ht=NULL;
  unsigned int buckets[head->hashAttempts];
  int logReadsPerLine=head->logReadsPerLine;
  int uBound=head->readsPerLine;
  int ha=head->hashAttempts;
  for(int i =0;i<ha;i++){
    buckets[i]=murmur3_32((const uint8_t *)&val, kSize, head->seeds[i]);
  }
  short tag=hashtag(val);
  for(int j=head->start;j<head->cur;j++){
  
    ht=head->TableArray[j];
    if(ht->bDel==2){
      continue;
    }
    for(int i =0; i<ha; i++) {
      int s=(buckets[i]%(ht->TableSize>>logReadsPerLine))<<logReadsPerLine;
      
      //call the new line before time amt of computation just cuz...
      //      __builtin_prefetch(ht->InnerTable[s+(tag&(uBound-1))]);
      //check this line
      for(int c=tag;c<uBound+tag;c++){
	//	__builtin_prefetch(ht->InnerTable[s+((c+1)&(uBound-1))]);
	int res=lookupQuery(ht, val, s+(c&(uBound-1)), tag);
	if(res==unk){ //unkown if in our not
	  continue;
	}
	if(res==notIn){ //is in
	  return 0;
	}

	int ret=setDelete(getEntPtr(ht->InnerTable[res]));
	if(ret&&!ht->bDel){
	  ht->tDeleted++;
	  if(ht->tDeleted>(ht->TableSize>>dClear)&&j!=(head->cur-1)&&!head->deleting&&pthread_mutex_trylock(&head->fMutex)){
	    unsigned long expec=0, newv=1;
	    int cret=__atomic_compare_exchange(&head->deleting,&expec, &newv, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
	    if(cret){
	    
	      ht->bDel=1;
	      head->delIndex=j;
	      pthread_mutex_unlock(&head->fMutex);

	    }
	  }
	}
      
	return ret; 
      }
    }
  }
  return 0;
}


int insertTable(HashTable* head,  int start, entry* ent, int tid){
  unsigned int buckets[head->hashAttempts];
  short tag;
  if(tid==-1){
    tag=getEntTag(ent);
  }
  else{
    tag=hashtag(getEntPtr(ent)->val);
    setTag(&ent, tag);
  }
  int logReadsPerLine=head->logReadsPerLine;
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
      if(ht->bDel==2){
	continue;
      }
      for(int i =0; i<ha; i++) {
	int s=(buckets[i]%(ht->TableSize>>logReadsPerLine))<<logReadsPerLine;

	//call the new line before time amt of computation just cuz...
	//	__builtin_prefetch(ht->InnerTable[s+(tag&(uBound-1))]);
        //check this line
	for(int c=tag;c<uBound+tag;c++){
	  //__builtin_prefetch(ht->InnerTable[s+((c+1)&(uBound-1))]);

	  //lookup value in sub table

	  int res=lookup(ht, ent,s+(c&(uBound-1)));
	  if(res==unk){ //unkown if in our not
	    continue;
	  }
	  if(res==in){ //is in
	    return 0;
	  }
	  if(ht->bDel){
	    //	continue;
	  }
	  entry* expected=NULL;
	  if(res==dUnk){
	    res=s+(c&(uBound-1));
	    return unDelete(getEntPtr(ht->InnerTable[res]));
	  }
	  else{
	    int cmp= __atomic_compare_exchange(ht->InnerTable+res,
					       &expected,
					       &ent,
					       1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
	    if(cmp){
	      return 1;
	    }
	    else{
	      if(getPtr(ht->InnerTable[res])){
	      if(getEntPtr(ht->InnerTable[res])->val==getEntPtr(ent)->val){
		return 0;
	      }
	      }
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
  return head->start;
}

void* delThread(void* targ){
  HashTable* head=(HashTable*)targ;
  int first=1;
  int skip=0;
  pthread_mutex_lock(&head->fMutex);
  SubTable* ht=NULL;
  while(1){
    /*        skip=0;
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
	  if(!skip){*/

    pthread_mutex_unlock(&head->dMutex);
    head->deleting=0;
    if(first){
      pthread_mutex_unlock(&head->fMutex);
    }
	  
    first=0;
    pthread_mutex_lock(&head->fMutex);
    //	  }
    if(head->stopResize){
      return NULL;
    }
    ht=head->TableArray[head->delIndex];
    //    printf("RESIZING: %d\n", head->delIndex);
    for(int i =0;i<ht->TableSize;i++){
      while(!getBool(ht->InnerTable[i])){
	setPtr(&ht->InnerTable[i]);
      }
      if(getPtr(ht->InnerTable[i])&&(!isDeleted(getEntPtr(ht->InnerTable[i])))){
	insertTable(head, head->delIndex+1, getPtr(ht->InnerTable[i]), -1);
	}
    }
  
    ht->bDel=2;
    if(head->TableArray[head->start]->bDel==2){
      head->start++;
    }
}
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

    //        printf("Changing value for initSize from %d to %d\n", 
    //	   InitSize, max(((int)((lines)*entPerLine)), entPerLine<<1));

    InitSize=max(((int)((lines)*entPerLine)),entPerLine<<1);
  }
  head->readsPerLine=(int)(entPerLine*lines);
  head->logReadsPerLine=logofint(head->readsPerLine);
  
  head->hashAttempts=HashAttempts;
  head->TableArray=(SubTable**)calloc(max_tables,sizeof(SubTable*));
  head->TableArray[0]=createTable(InitSize);
  
  head->cur=1;
  head->start=0;
  head->stopResize=0;
  pthread_mutex_init(&head->fMutex,NULL);
  pthread_mutex_init(&head->dMutex,NULL);
  pthread_mutex_lock(&head->dMutex);

  pthread_create(&head->dThread,NULL, delThread,(void*)head);
  pthread_mutex_lock(&head->dMutex);
  return head;
}

static SubTable* 
createTable(int tsize){
  SubTable* ht=(SubTable*)calloc(1,sizeof(SubTable));
  ht->TableSize=tsize;
  ht->InnerTable=(entry**)aligned_alloc(lineSize,tsize*sizeof(entry*));
  memset(ht->InnerTable, 0, tsize*sizeof(entry*));
  return ht;
}

