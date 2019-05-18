#ifndef _HASHTABLE_H_
#define _HASHTABLE_H_

typedef struct TableHead HashTable;

typedef struct entry{
  unsigned long val;
}entry;

// free hash table when done
double freeAll(HashTable* head, int last);

// see if entry is in the table
int checkTableQuery(HashTable* head, entry* ent);

// initialize a new main hashtable
TableHead* initTable(HashTable* head, int InitSize, int HashAttempts);


// return 1 if inserted, 0 if already there
int insertTable(HashTable* head,  int start, entry* ent);


#endif
