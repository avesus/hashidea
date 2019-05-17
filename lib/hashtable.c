#include "hashtable.h"
#include "hash.h"


int lookupQuery(HashTable* ht, entry* ent, unsigned int seeds){
  unsigned int s=murmur3_32(&ent->val, 8, seeds)%ht->TableSize;
  if(ht->InnerTable[s]==NULL){
    return notIn;
  }
  else if(ent->val==ht->InnerTable[s]->val){
    return s;
  }
  return unk;
}

int checkTableQuery(TableHead* head, entry* ent, unsigned int* seeds, int hash_attempts){
  h_table* ht=NULL;
  for(int j=0;j<head->cur;j++){
    ht=head->tt[j];
    for(int i =0;i<hash_attempts;i++){
      int res=lookupQuery(ht->InnerTable, ent, seeds[i]);
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


void freeTable(h_table* ht){
  free(ht);
}


//check if entry for a given hashing vector is in a table
int lookup(HashTable* ht, entry* ent, unsigned int seeds){


  unsigned int s= murmur3_32(&ent->val, 8, seeds)%ht->TableSize;
  if(ht->InnerTable[s]==NULL){
    return s;
  }
  else if(ht->InnerTable[s]->val==ent->val){
    return in;
  }
  return unk;
}



int addDrop(TableHead* head, HashTable* toadd, int toadd_slot, entry* ent, unsigned int* seeds, int hash_attempts){
  h_table* expected=NULL;
  int res = __atomic_compare_exchange(&head->TableArray[toadd_slot] ,&expected, &toadd, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
  if(res){
    int newSize=toadd_slot+1;
    __atomic_compare_exchange(&head->cur,
			      &toadd_slot,
			      &newSize,
			      1,__ATOMIC_RELAXED, __ATOMIC_RELAXED);
    insertTable(head, 1, ent, seeds, hash_attempts);
  }
  else{
    freeTable(toadd);
    int newSize=toadd_slot+1;
    __atomic_compare_exchange(&head->cur,
			      &toadd_slot,
			      &newSize,
			      1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);

    insertTable(head, 1, ent, seeds, hash_attempts);
  }
  return 0;
}



int insertTable(TableHead* head,  int start, long int val, unsigned int* seeds, int hash_attempts){
  int startCur=head->cur;
  HashTable* ht=NULL;
  for(int j=start;j<head->cur;j++){
    ht=head->TableArray[j];
    for(int i =0;i<hash_attempts;i++){
      int res=lookup(ht, ent, seeds[i]);
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
    startCur=head->cur;
  }
  HashTable* new_table=createTable(head->TableArray[startCur-1]->TableSize<<1);
  addDrop(head, new_table, startCur, ent, seeds, hash_attempts);
}


TableHead* initTable(int init_size){
  TableHead* head=(TableHead*)malloc(sizeof(TableHead));
  head->TableArray=(HashTable*)malloc(max_tables*sizeof(HashTable*));
  head->TableArray[0]=createTable(init_size);
  head->cur=1;
}

HashTable* createTable(int tsize){
  HashTable* ht=(HashTable*)malloc(sizeof(HashTable));
  ht->TableSize=tsize;
  ht->InnerTable=(entry**)malloc(sizeof(entry*)*(ht->TableSize));
  return ht;
}
