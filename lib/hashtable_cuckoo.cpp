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


#include </home/noah/programs/hashidea/other_hash/libcuckoo/libcuckoo/cuckoohash_map.hh>


const char* tablename = "cuckoo/cpp/remain:V";
const char* shortname = "OMR:V";

typedef cuckoohash_map<int, int> T;
//a sub table (this should be hidden)
typedef struct SubTable {
} SubTable;

// head of cache: this is the main hahstable
typedef struct HashTable{
  T Table;
  HashTable(int size) : Table(size) {  }
  
  //int_str_table* tbl;
} HashTable;


// free hash table when done
double freeAll(HashTable* head, int last, int verbose){
  return 0;
}

// see if entry is in the table
int checkTableQuery(HashTable* head, unsigned long val){
  int out;
  return head->Table.find((int)val,out);
}

// initialize a new main hashtable
HashTable* initTable(HashTable* head, int InitSize, int HashAttempts, int numThreads, unsigned int * seeds){

  head=new HashTable(InitSize);
  return head;
}


// return 1 if inserted, 0 if already there
int insertTable(HashTable* head,  int start, entry* ent, int tid){
  return head->Table.insert((int)ent->val, (int)ent->val);
}

int getStart(HashTable* head){
  return 0;
}


int deleteVal(HashTable* head, unsigned long val){
  return 0;
}
////////////////////////////////////////////////////////////////
// used only for testing and timing



//#endif
