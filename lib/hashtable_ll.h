#ifndef _HASHTABLE_LL_H_
#define _HASHTABLE_LL_H_

typedef struct HashTable HashTable;


typedef struct node{
  volatile unsigned long val;
  volatile struct pointer next;
}node;


// free hash table when done
double freeAll(HashTable* head, int last, int verbose);

// see if entry is in the table
int checkTableQuery(HashTable* head, unsigned long val);

// initialize a new main hashtable
HashTable* initTable(HashTable* head, int InitSize, int HashAttempts, int numThreads, unsigned int* seeds);


// return 1 if inserted, 0 if already there
int insertTable(HashTable* head, int start, node* ent, int tid);

int getStart(HashTable* head);

////////////////////////////////////////////////////////////////
// used only for testing and timing

extern const char* tablename;

#endif
