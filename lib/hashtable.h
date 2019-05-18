#ifndef _HASHTABLE_H_
#define _HASHTABLE_H_

typedef struct entry{
  unsigned long val;
}entry;

//a sub table (this should be hidden)
typedef struct HashTable {
  entry** InnerTable; //rows (table itself)
  int TableSize; //size
} HashTable;

// head of cache: this is the main hahstable
typedef struct TableHead{
  HashTable** TableArray; //array of tables
  unsigned int * seeds;
  int hashAttempts;
  int cur; //current max index (max exclusive)
} TableHead;


// free hash table when done
double freeAll(TableHead* head, int last);

// see if entry is in the table
int checkTableQuery(TableHead* head, entry* ent);

// initialize a new main hashtable
TableHead* initTable(TableHead* head, int InitSize, int HashAttempts);


// return 1 if inserted, 0 if already there
int insertTable(TableHead* head,  int start, entry* ent);


#endif
