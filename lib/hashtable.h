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

#ifndef _HASHTABLE_H_
#define _HASHTABLE_H_

#define max_tables 64 //max tables to create

//return values for checking table
#define notIn 0 
#define in -1
#define unk -2

typedef struct entry{
  unsigned long val;
}entry;

//a table
typedef struct HashTable{
  entry** InnerTable; //rows (table itself)
  int TableSize; //size
}HashTable;

//head of cache
typedef struct TableHead{
  HashTable** TableArray; //array of tables
  unsigned int * seeds;
  int hashAttempts;
  int cur; //current max index (max exclusive)
}TableHead;


double freeAll(TableHead* head, int last);

int checkTableQuery(TableHead* head, entry* ent);

unsigned int* initSeeds(int HashAttempts);

int lookupQuery(HashTable* ht, entry* ent, unsigned int seeds);

TableHead* initTable(TableHead* head, int InitSize, int HashAttempts);

HashTable* createTable(int hsize);

// free table
void freeTable(HashTable* table);

// return 1 if inserted, 0 if already there
int insertTable(TableHead* head,  int start, entry* ent);

//creates new hashtable in the tablearray
int addDrop(TableHead* head, HashTable* toadd, int AddSlot, entry* ent);

//lookup function in insertTrial to check a given inner table
int lookup(HashTable* ht, entry* ent, unsigned int seeds);

#endif
