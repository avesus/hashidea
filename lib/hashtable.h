#ifndef _HASHTABLE_H_
#define _HASHTABLE_H_

// create table with initial size 
HashTable* createTable(size_t hsize);

// free table
void freeTable(HashTable* table);

// return 1 if inserted, 0 if already there
int insertTrial(HashTable* table, long int val);

#endif
