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

// head of cache: this is the main hahstable
typedef struct HashTable{
  SubTable** TableArray; //array of tables
  unsigned int * seeds;
  int hashAttempts;
  int cur; //current max index (max exclusive)
} HashTable;


#define max_tables 64 //max tables to create

//return values for checking table.  Returned by lookupQuery
#define kSize 4
#define notIn -3 
#define in -1
#define unk -2
#define deleted 1
#define undeleting 2
#define finishUpper 4
#define lowBits 7


#define LOST_DELETE_RACE 2
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
  return ((unsigned long)ptr)&lowBits;
}

//gets pointer by anding off the first 4 bits from a ptr
entry* getPtr(entry* ptr){
  unsigned long mask=lowBits;
  return (entry*)((((unsigned long)ptr)&(~mask)));
}

//set count value for a pointer
int setPtr(entry** ptr, char oldBits, char bits){
  entry* newEnt=(entry*)(((unsigned long)(getPtr(*ptr)))|bits);
  entry* exEnt= (entry*)(((unsigned long)(getPtr(*ptr)))|oldBits);
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

  for(int j=0;j<head->cur;j++){

    ht=head->TableArray[j];
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
static int lookup(SubTable* ht, entry* ent, unsigned int s){

  if(ht->InnerTable[s]==NULL){
    return s;
  }
  if(getBool(ht->InnerTable[s])==deleted){
    return s;
  }
  else if(getPtr(ht->InnerTable[s])->val==ent->val){
    return in;
  }
  
  return unk;
}

static int lookupDelete(SubTable* ht, entry* ent, unsigned int s){

  if(ht->InnerTable[s]==NULL){
    return notIn;
  }
  else if(getPtr(ht->InnerTable[s])->val==ent->val){
    return s;
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
  for(int j=0;j<head->cur;j++){

    ht=head->TableArray[j];
    for(int i =0; i<head->hashAttempts; i++) {
      if(!j){
	buckets[i]=murmur3_32((const uint8_t *)&val, kSize, head->seeds[i]);
      }
      int res=lookupQuery(ht, val, buckets[i]%ht->TableSize);
      if(res==unk){ //unkown if in our not
	continue;
      }
      if(res==notIn){ //is in
	return 0;
      }
      while(getPtr(ht->InnerTable[res])->val==val&&getBool(ht->InnerTable[res])!=deleted){
	setPtr(&ht->InnerTable[res], getBool(ht->InnerTable[res]), 1);
      }
    }
  }
  return 0;
}


int deleteAdd(HashTable* head, int start, int hashLoc, entry** pos, entry* ent, unsigned int* buckets){
  SubTable* ht;
    for(int j=start;j<head->cur;j++){
      ht=head->TableArray[j];
      for(int i=(hashLoc*(start==j));i<head->hashAttempts;i++){
	int res=lookupDelete(ht, ent,buckets[i]%ht->TableSize);
	if(res==unk){ //unkown if in our not
	  continue;
	}
	if(res==notIn){ //is in
	  goto downLoop;
	}
	int bits=getBool(ht->InnerTable[res]);
	if(!bits){
	      setPtr(pos, undeleting, deleted);
	      return 0;
	}
	else if(bits==deleted){
	  continue;
	}
	//feels weird to kill something with more progress than us
	//but necessary to force same path for all
	else if (bits==undeleting||bits==finishUpper){
	retryDelete:
	  if(setPtr(&ht->InnerTable[res], bits, deleted)){
	    continue;
	  }
	  else{
	    bits = getBool(ht->InnerTable[res]);
	    if(!bits){
	      //given that in undeleting state only can be set to delete by other
	      //dont need to verify return value
	      setPtr(pos, undeleting, deleted);
	      return 0;
	      //want to unset self
	    }
	    else if(bits==deleted){
	      //case another thread unset at same time
	      continue;
	    }
	    else {
	      //i really think this is best way to do it (so the continues still work!)
	      goto retryDelete;
	    }
	  }
	}
      }
    }
 downLoop:
    if(!setPtr(pos,undeleting, finishUpper)){
      //only way to get here is someone else set me to deleted so can just return
      return 0;
    }
    for(int j=start;j>=0;j--){
      for(int i=0;i<max(hashLoc, head->hashAttempts*(j!=start));i++){
	int res=lookupDelete(ht, ent,buckets[i]%ht->TableSize);
	if(res==unk){ //unkown if in our not
	  continue;
	}
	if(res==notIn){ //this case should NEVER happen!
	  fprintf(stderr,"If this happened I really dont understand the control flow...\nThis should be impossible.\n");
	  exit(0);
	}
	int bits=getBool(ht->InnerTable[res]);
	if(!bits){
	  setPtr(pos, undeleting, deleted);
	  return 0;
	}
	else if(bits==deleted){
	  continue;
	}
	
	else if (bits==undeleting||bits==finishUpper){
	  setPtr(pos, undeleting, deleted);
	  return 0;
	}
      }
    }
   
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

      for(int i =0; i<head->hashAttempts; i++) {
	int res=lookup(ht, ent,buckets[i]%ht->TableSize);
	if(res==unk){ //unkown if in our not
	  continue;
	}
	if(res==in){ //is in
	  return 0;
	}

	entry* expected=NULL;
	int delAdd=0;
	if(ht->InnerTable[res]){
	  setPtr(&ent,0,2);
	  setPtr(&expected, 0, 1);
	  delAdd=1;
	}
      int cmp= __atomic_compare_exchange(ht->InnerTable+res,
					 &expected,
					 &ent,
					 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
      if(cmp){
	if(delAdd){
	  if(!deleteAdd(head, start, i, &ht->InnerTable[res], ent, (unsigned int*)buckets)){
	    return LOST_DELETE_RACE;
	  }
	}
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
  return 0;
}

HashTable* initTable(HashTable* head, int InitSize, int HashAttempts, int numThreads, unsigned int* seeds, double lines){
  head=(HashTable*)calloc(1,sizeof(HashTable));
  head->seeds=seeds;
  head->hashAttempts=HashAttempts;
  head->TableArray=(SubTable**)calloc(max_tables,sizeof(SubTable*));
  head->TableArray[0]=createTable(InitSize);
  head->cur=1;
  return head;
}

static SubTable* 
createTable(int tsize){
  SubTable* ht=(SubTable*)malloc(sizeof(SubTable));
  ht->TableSize=tsize;
  ht->InnerTable=(entry**)calloc((ht->TableSize),sizeof(entry*));
  return ht;
}