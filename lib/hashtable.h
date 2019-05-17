#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>


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
  int_ent** InnerTable; //rows (table itself)
  int TableSize; //size
}HashTable;

//head of cache
typedef struct TableHead{
  HashTable** TableArray; //array of tables
  int cur; //current max index (max exclusive)
}TableHead;


int checkTableQuery(TableHead* head, entry* ent, unsigned int* seeds, int hash_attempts);

int lookupQuery(HashTable* ht, entry* ent, unsigned int seeds);

TableHead* initTable(int init_size);

HashTable* createTable(size_t hsize);

// free table
void freeTable(HashTable* table);

// return 1 if inserted, 0 if already there
int insertTable(TableHead* head,  int start, entry* ent, unsigned int* seeds, int hash_attempts);

//creates new hashtable in the tablearray
int addDrop(TableHead* head, HashTable* toadd, int toadd_slot, entry* ent, unsigned int* seeds);

//lookup function in insertTrial to check a given inner table
int lookup(HashTable* ht, entry* ent, unsigned int seeds);

#endif
