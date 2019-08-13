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
  int lineReads; //amount of reads to do after each hash
                 //(i.e 4 reads is check 4 entries after getting slot so does not mean
                 //4 entire lines)
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
static inline int lookup(SubTable* ht, entry* ent, unsigned int s);

int deleteVal(HashTable* head, unsigned long val){
  return 1;
}




//returns tag from given entry (highest byte)
inline char getEntTag(entry* ent){

  return (char)(((unsigned long)ent)>>56);
}

//returns the ptr for a given entry (0s out the tag bits)
inline entry*  getEntPtr(entry* ent){
  entry* ret= (entry*)((((unsigned long)ent)<<8)>>8);
  return (entry*)((((unsigned long)ent)<<8)>>8);
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
static inline int lookupQuery(SubTable* ht, unsigned long val, unsigned int s){

  if(ht->InnerTable[s]==NULL){
    return notIn;
  }
  if(hashtag(val)!=getEntTag(ht->InnerTable[s])){
    return unk;
  }
  if(val==getEntPtr(ht->InnerTable[s])->val){
    return s;
  }
  return unk;
}

int checkTableQuery(HashTable* head, unsigned long val){
  SubTable* ht=NULL;
  unsigned int buckets[head->hashAttempts];
  //  for(int i =0;i<head->hashAttempts;i++){
    //    buckets[i]=murmur3_32((const uint8_t *)&val, kSize, head->seeds[i]);
  //  }
  int numLines=head->lineReads>>logLineSize;
  for(int j=0;j<head->cur;j++){

    ht=head->TableArray[j];
    for(int i =0; i<head->hashAttempts; i++) {
      if(!j){
      buckets[i]=murmur3_32((const uint8_t *)&val, kSize, head->seeds[i]);
      }
            int s=(buckets[i]%(ht->TableSize>>head->lineReads))<<head->lineReads;
      ht->InnerTable[s];

      for(int l=0;l<numLines;l++){
	int uBound=min((head->lineReads-(entPerLine*l)), entPerLine);
	if(l+1<numLines){
	  ht->InnerTable[s+entPerLine];
	}
	else if((l+1)==numLines&&j&&(i+1)<head->hashAttempts){
	  ht->InnerTable[(buckets[i+1]%(ht->TableSize>>logLineSize))<<logLineSize];
	}
	for(int c=s;c<s+uBound;c++){
      int res=lookupQuery(ht, val, c);
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

  //if slot is null just return its empty
  if(ht->InnerTable[s]==NULL){
    return s;
  }

  //if slot is not null first check tags (if no match then return unk)
  if(getEntTag(ht->InnerTable[s])!=getEntTag(ht->InnerTable[s])){
    return unk;
  }

  //if tags match bring entry value into cache and check
  if(getEntPtr(ht->InnerTable[s])->val==getEntPtr(ent)->val){
    return in;
  }

  //if no match for same tag return unk
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
    //    insertTable(head, 1, ent, 0);
  }
  else{
    freeTable(toadd);
    int newSize=AddSlot+1;
    __atomic_compare_exchange(&head->cur,
			      &AddSlot,
			      &newSize,
			      1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);

    //    insertTable(head, 1, ent, 0);
  }
  return 0;
}



int insertTable(HashTable* head,  int start, entry* ent, int tid){
  unsigned int buckets[head->hashAttempts];

  //create the tag for the entry and set it
  char tag=hashtag(ent->val);
  setTag(&ent, tag);

  //figure entry is in cache now so might aswell get first hash here
  buckets[0]=murmur3_32((const uint8_t *)&getEntPtr(ent)->val, kSize, head->seeds[0]);
  SubTable* ht=NULL;
  int LocalCur=head->cur;


  int numLines=1+((head->lineReads-1)>>logLineSize);
  while(1){
  for(int j=start;j<head->cur;j++){
    ht=head->TableArray[j];

    for(int i =0; i<head->hashAttempts; i++) {
      //storing hash values in buckets so dont need to recompute each loop through j
      if(!j&&i){
	buckets[i]=murmur3_32((const uint8_t *)&getEntPtr(ent)->val, kSize, head->seeds[i]);
      }

      //divide by lineReads (dont give lines some value so that lineReads is like 17
      //or something bad, try 1,2,4,8,16,24,etc.. (basically if less that lineSize/entry Size
      //keep power of 2, otherwise keep multiple of entry size) also I assume gcc will turn
      //the divide into >>, if not probably better to precomputer log and use that
      int s=(buckets[i]%(ht->TableSize/head->lineReads))*head->lineReads;

      //call the new line before time amt of computation just cuz...
      ht->InnerTable[s];
      for(int l=0;l<numLines;l++){

	//how many entries in the given line to check (this does allow for setting reads to
	//12 or something
	int uBound=min(head->lineReads-(entPerLine*l), entPerLine);

	//if we will be checking next line get it now
	if(l+1<numLines){
	  ht->InnerTable[s+entPerLine];
	}

	//if this is the last line for this hash and we are doing another hash
	//get it
	else if((l+1)==numLines&&j&&(i+1)<head->hashAttempts){
	  ht->InnerTable[(buckets[i+1]%(ht->TableSize>>logLineSize))<<logLineSize];
	}

	//check this line
	for(int c=s;c<s+uBound;c++){
	  int res=lookup(ht, ent,c);
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
	if(getEntPtr(ht->InnerTable[res])->val==getEntPtr(ent)->val){
	  return 0;
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
  return 0;
}

HashTable* initTable(HashTable* head, int InitSize, int HashAttempts, int numThreads, unsigned int* seeds, double lines){
  head=(HashTable*)calloc(1,sizeof(HashTable));
  head->seeds=seeds;
  if(InitSize<max(((int)((lines)*entPerLine)),entPerLine<<1)){
    printf("Changing value for initSize from %d to %d\n", InitSize,max(((int)((lines)*entPerLine)),entPerLine<<1));
    InitSize=max(((int)((lines)*entPerLine)),entPerLine<<1);
  }

  head->lineReads=(int)(entPerLine*lines);
  head->hashAttempts=HashAttempts;
  head->TableArray=(SubTable**)calloc(max_tables,sizeof(SubTable*));
  head->TableArray[0]=createTable(InitSize);
  head->cur=1;
  printf("%d, %d, %d, %d, %lf\n", lineSize, logLineSize, entPerLine, head->lineReads, lines);
  return head;
}

static SubTable* 
createTable(int tsize){
  SubTable* ht=(SubTable*)malloc(sizeof(SubTable));
  ht->TableSize=tsize;
  ht->InnerTable=(entry**)calloc((ht->TableSize),sizeof(entry*));
  return ht;
}
