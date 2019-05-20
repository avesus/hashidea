#ifndef _HASHTABLE_H_
#define _HASHTABLE_H_

typedef struct HashTable HashTable;

typedef struct entry{
  unsigned long val;
}entry;

// free hash table when done
double freeAll(HashTable* head, int last, int verbose);

// see if entry is in the table
int checkTableQuery(HashTable* head, entry* ent);

// initialize a new main hashtable
HashTable* initTable(HashTable* head, int InitSize, int HashAttempts, int numThreads);


// return 1 if inserted, 0 if already there
int insertTable(HashTable* head,  int start, entry* ent, int tid);

int getStart(HashTable* head);

#endif
