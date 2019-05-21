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
#define notIn -3 
#define in -1
#define unk -2

// create a sub table
static SubTable* createTable(int hsize);
// free a subtable 
static void freeTable(SubTable* table);

//creates new hashtable in the tablearray
static int addDrop(HashTable* head, SubTable* toadd, int AddSlot, entry* ent);

//lookup function in insertTrial to check a given inner table
static int lookup(SubTable* ht, entry* ent, unsigned int seeds);



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

	free(ht->InnerTable[j]);
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
    free(head);
  }
  return count/totalSize;  
}

static void 
freeTable(SubTable* ht){
  free(ht);
}


//check if entry for a given hashing vector is in a table
static int lookup(SubTable* ht, entry* ent, unsigned int seed){

  unsigned int s= murmur3_32((const uint8_t *)&ent->val, sizeof(ent->val), seed)%ht->TableSize;
  if(ht->InnerTable[s]==NULL){
    return s;
  }
  else if(ht->InnerTable[s]->val==ent->val){
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

  SubTable* ht=NULL;
  int LocalCur=head->cur;
  for(int j=start;j<head->cur;j++){
    ht=head->TableArray[j];
    for(int i =0;i<head->hashAttempts;i++){
      int res=lookup(ht, ent, head->seeds[i]);
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
  SubTable* new_table=createTable(head->TableArray[LocalCur-1]->TableSize<<1);
  addDrop(head, new_table, LocalCur, ent);
}


static unsigned int*
initSeeds(int HashAttempts){
  unsigned int * seeds=(unsigned int*)malloc(sizeof(unsigned int)*HashAttempts);
  for(int i =0;i<HashAttempts;i++){
    seeds[i]=random();
  }
  return seeds;
}

int getStart(HashTable* head){
  return 0;
}

HashTable* initTable(HashTable* head, int InitSize, int HashAttempts, int numThreads){
  if(!head){
    head=(HashTable*)calloc(1,sizeof(HashTable));
    head->seeds=initSeeds(HashAttempts);
    head->hashAttempts=HashAttempts;
  }
  head->TableArray=(SubTable**)calloc(max_tables,sizeof(SubTable*));
  head->TableArray[0]=createTable(InitSize);
  head->cur=1;
  return head;
}

static SubTable* 
createTable(int tsize){
  SubTable* ht=(SubTable*)malloc(sizeof(SubTable));
  ht->TableSize=tsize;
  ht->InnerTable=(entry**)calloc(sizeof(entry*),(ht->TableSize));
  return ht;
}
