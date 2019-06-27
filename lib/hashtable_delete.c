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
#define del -4
#define notIn -3 
#define in -1
#define unk -2
#define deleted 1
#define claimed 2
#define ptr 3
#define min(X, Y)  ((X) < (Y) ? (X) : (Y))



int getBool(entry* ent){
  return ((unsigned long)ent)&ptr;
}

entry* getPtr(entry* ent){
  unsigned long mask=ptr;
  return (entry*)(((unsigned long)ent)&(~mask));
}

int setPtr(entry** ent, int val){
  entry* newEnt=(entry*)((unsigned long)(*ent)|val);
  int temp=getBool(*ent);
  if(temp&val){
    return 0;
  }
  entry* exEnt= (entry*)((unsigned long)(getPtr(*ent))|temp);;
  return __atomic_compare_exchange(ent,&exEnt, &newEnt, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}
void unsetPtr(entry** ent, int val){
  entry* newEnt=(entry*)((unsigned long)(*ent)^val);
  *ent=newEnt;
}

// create a sub table
static SubTable* createTable(int hsize);
// free a subtable 
static void freeTable(SubTable* table);

//creates new hashtable in the tablearray
static int addDrop(HashTable* head, SubTable* toadd, int AddSlot, entry* ent);

//lookup function in insertTrial to check a given inner table
static int lookup(SubTable* ht, entry* ent, unsigned int s);


static int 
lookupQuery(SubTable* ht, unsigned long val, unsigned int s){

  if(getPtr(ht->InnerTable[s])==NULL){
    return notIn;
  }
  else if(val==getPtr(ht->InnerTable[s])->val&&getBool(ht->InnerTable[s])!=deleted){
    return s;
  }
  return unk;
}


int deleteVal(HashTable* head, unsigned long val){
  SubTable* ht=NULL;
  unsigned int buckets[10];
  for(int i =0;i<head->hashAttempts;i++){
    buckets[i]=murmur3_32((const uint8_t *)&val, kSize, head->seeds[i]);
  }
  for(int j=0;j<head->cur;j++){
    ht=head->TableArray[j];
    for(int i =0; i<min((j<<1)+1,head->hashAttempts); i++) {
      int res=lookupQuery(ht, val, buckets[i]%ht->TableSize);
      if(res==unk){
	continue;
      }
      if(res==notIn){
	return 0;
      }
      return setPtr(&ht->InnerTable[res], deleted);
    }
  }
}
/*
  find unclaimed deleted
  claim in CAS (bool) <-  need to set claimed boot on entry to be inserted (so get new val and claimed in one
  if(win)
  go to null, in or finished (after claim store temp val of max)
  decend back to self, if find self greater than expected loc, kill self
*/



int constrainedCheckTableQuery(HashTable* head, unsigned long val, int start, int end){
  SubTable* ht=NULL;
  unsigned int buckets[10];
  for(int i =0;i<head->hashAttempts;i++){
    buckets[i]=murmur3_32((const uint8_t *)&val, kSize, head->seeds[i]);
  }
  for(int j=start;j<end;j++){
    ht=head->TableArray[j];
    for(int i =0; i<min((j<<1)+1,head->hashAttempts); i++) {
      int res=lookupQuery(ht, val, buckets[i]%ht->TableSize);
      if(res==unk){ //unkown if in our not
	continue;
      }
      if(res==notIn){ //is not in
	return 0;
      }
      //not in (we got null so it would have been there)
      return 1;
    }
  }
  for(int j=end;j<start;j++){
    ht=head->TableArray[j];
    for(int i =0; i<min((j<<1)+1,head->hashAttempts); i++) {
      int res=lookupQuery(ht, val, buckets[i]%ht->TableSize);
      if(res==unk){ //unkown if in our not
	continue;
      }
      if(res==notIn){ //is not in
	return 0;
      }
      //not in (we got null so it would have been there)
      return 1;
    }
  }
  return 0;
}
int checkTableQuery(HashTable* head, unsigned long val){
  SubTable* ht=NULL;
  unsigned int buckets[10];
  for(int i =0;i<head->hashAttempts;i++){
    buckets[i]=murmur3_32((const uint8_t *)&val, kSize, head->seeds[i]);
  }
  for(int j=0;j<head->cur;j++){
    ht=head->TableArray[j];
    for(int i =0; i<min((j<<1)+1,head->hashAttempts); i++) {
      int res=lookupQuery(ht, val, buckets[i]%ht->TableSize);
      if(res==unk){ //unkown if in our not
	continue;
      }
      if(res==notIn){ //is not in
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
      if(getPtr(ht->InnerTable[j])!=NULL){
	//free(getPtr(ht->InnerTable[j]));
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

  if(last){
    free(head->seeds);
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

  if(getPtr(ht->InnerTable[s])==NULL){
    return s;
  }
  else if(getPtr(ht->InnerTable[s])->val==ent->val&&getBool(ht->InnerTable[s])!=deleted){
    return in;
  }
  else if(getBool(ht->InnerTable[s])==deleted){
    return del;
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
    insertTable(head, 1, ent, 0);
  }
  else{
    freeTable(toadd);
    int newSize=AddSlot+1;
    __atomic_compare_exchange(&head->cur,
			      &AddSlot,
			      &newSize,
			      1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);

    insertTable(head, 1, ent, 0);
  }
  return 0;
}



int insertTable(HashTable* head,  int start, entry* ent, int tid){
  printf("Starting: %p->%d = %lu\n", ent, getBool(ent), ent->val);
  SubTable* ht=NULL;
  int LocalCur=head->cur;
  unsigned int buckets[10];
  for(int i =0;i<head->hashAttempts;i++){
    buckets[i]=murmur3_32((const uint8_t *)&ent->val, kSize, head->seeds[i]);
  }

  for(int j=start;j<head->cur;j++){
    ht=head->TableArray[j];

    /*  printf("%p, %p, %d\n", ht->InnerTable[4], getPtr(ht->InnerTable[4]), getBool(ht->InnerTable[4]));
	setPtr(&ht->InnerTable[4], deleted);
	printf("%p, %p, %d\n", ht->InnerTable[4], getPtr(ht->InnerTable[4]), getBool(ht->InnerTable[4]));
	setPtr(&ht->InnerTable[4], claimed);
	printf("%p, %p, %d\n", ht->InnerTable[4], getPtr(ht->InnerTable[4]), getBool(ht->InnerTable[4]));
	unsetPtr(&ht->InnerTable[4], claimed);
	printf("%p, %p, %d\n", ht->InnerTable[4], getPtr(ht->InnerTable[4]), getBool(ht->InnerTable[4]));
	//   unsetPtr(&ht->InnerTable[4], deleted);
	setPtr(&ent, claimed|deleted);
	entry* expected=NULL;
	setPtr(&expected,deleted);
	int cmp= __atomic_compare_exchange((ht->InnerTable+4),
	&expected,
	&ent,
	1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
	printf("%p, %p, %d\n", ht->InnerTable[4], getPtr(ht->InnerTable[4]), getBool(ht->InnerTable[4]));
	exit(0);*/


    
    for(int i =0; i<min((j<<1)+1,head->hashAttempts); i++) {
      int res=lookup(ht, ent,buckets[i]%ht->TableSize);
      if(res==unk){ //unkown if in our not
	continue;
      }
      if(res==in){ //is in
	return 0;
      }
      if(res==del){
	int b=buckets[i]%ht->TableSize;
	printf("Adding to deleted slot: %lu to %lu\n", ent->val, getPtr(ht->InnerTable[b])->val);
	printf("%p->%d vs %p->%d\n", ent, getBool(ent), ht->InnerTable[b], getBool(ht->InnerTable[b]));
	setPtr(&ent, claimed|deleted);
	entry* expected=NULL;
	setPtr(&expected,deleted);
	int cmp= __atomic_compare_exchange((ht->InnerTable+b),
					   &expected,
					   &ent,
					   1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
	if(cmp){
	  if(constrainedCheckTableQuery(head, getPtr(ent)->val, j, head->cur)){
	    unsetPtr(&ht->InnerTable[b], claimed);
	  }
	  return 1;
	}
	else{
	  unsetPtr(&ent, claimed|deleted);
	  if(getPtr(ht->InnerTable[b])->val==getPtr(ent)->val){
	    return 0;
	  }
	}
	
      }
      else{
	entry* expected=NULL;
	int cmp= __atomic_compare_exchange((ht->InnerTable+res),
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
    }
    LocalCur=head->cur;
  }
  SubTable* new_table=createTable(head->TableArray[LocalCur-1]->TableSize<<1);
  addDrop(head, new_table, LocalCur, ent);
}


int getStart(HashTable* head){
  return 0;
}

HashTable* initTable(HashTable* head, int InitSize, int HashAttempts, int numThreads, unsigned int* seeds){
  head=(HashTable*)calloc(1,sizeof(HashTable));
  head->seeds=seeds;
  if(HashAttempts>10){
    printf("Changing value for hashattempts from %d to 10\n", HashAttempts);
    HashAttempts=10;
  }
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
