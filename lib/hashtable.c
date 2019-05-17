#include "hashtable.h"
#include "hash.h"


int lookupQuery(HashTable* ht, entry* ent, unsigned int seed){
  unsigned int s=murmur3_32((const uint8_t *)&ent->val, 8, seed)%ht->TableSize;
  if(ht->InnerTable[s]==NULL){
    return notIn;
  }
  else if(ent->val==ht->InnerTable[s]->val){
    return s;
  }
  return unk;
}

int checkTableQuery(TableHead* head, entry* ent){
  HashTable* ht=NULL;
  for(int j=0;j<head->cur;j++){
    ht=head->TableArray[j];
    for(int i =0;i<head->hashAttempts;i++){
      int res=lookupQuery(ht, ent, head->seeds[i]);
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

double freeAll(TableHead* head, int last){
  
  HashTable* ht=NULL;
  float count=0;

  for(int i = 0;i<head->cur; i++){
    ht=head->TableArray[i];
    for(int j =0;j<ht->TableSize;j++){
      if(ht->InnerTable[j]!=NULL){
	free(ht->InnerTable[j]);
	count++;
      }
    }
    free(ht);
  }

  
  
  free(head->TableArray);  
  if(last){
    free(head->seeds);
    free(head);
  }
  return (double)((double)count)/(((double)(1<<head->cur)-1));
}

void freeTable(HashTable* ht){
  free(ht);
}


//check if entry for a given hashing vector is in a table
int lookup(HashTable* ht, entry* ent, unsigned int seed){

  unsigned int s= murmur3_32((const uint8_t *)&ent->val, 8, seed)%ht->TableSize;
  if(ht->InnerTable[s]==NULL){
    return s;
  }
  else if(ht->InnerTable[s]->val==ent->val){
    return in;
  }
  return unk;
}



int addDrop(TableHead* head, HashTable* toadd, int AddSlot, entry* ent){
  HashTable* expected=NULL;
  int res = __atomic_compare_exchange(&head->TableArray[AddSlot] ,&expected, &toadd, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
  if(res){
    int newSize=AddSlot+1;
    __atomic_compare_exchange(&head->cur,
			      &AddSlot,
			      &newSize,
			      1,__ATOMIC_RELAXED, __ATOMIC_RELAXED);
    insertTable(head, 1, ent);
  }
  else{
    freeTable(toadd);
    int newSize=AddSlot+1;
    __atomic_compare_exchange(&head->cur,
			      &AddSlot,
			      &newSize,
			      1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);

    insertTable(head, 1, ent);
  }
  return 0;
}



int insertTable(TableHead* head,  int start, entry* ent){

  HashTable* ht=NULL;
  int LocalCur=head->cur;
  for(int j=start;j<head->cur;j++){
    ht=head->TableArray[j];
    for(int i =0;i<head->hashAttempts;i++){
      int res=lookup(ht, ent, head->seeds[i]);
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
    LocalCur=head->cur;
  }
  HashTable* new_table=createTable(head->TableArray[LocalCur-1]->TableSize<<1);
  addDrop(head, new_table, LocalCur, ent);
}


unsigned int*
initSeeds(int HashAttempts){
  unsigned int * seeds=(unsigned int*)malloc(sizeof(unsigned int)*HashAttempts);
  for(int i =0;i<HashAttempts;i++){
    seeds[i]=random();
  }
  return seeds;
}


TableHead* initTable(TableHead* head, int InitSize, int HashAttempts){
  if(!head){
    head=(TableHead*)calloc(1,sizeof(TableHead));
    head->seeds=initSeeds(HashAttempts);
    head->hashAttempts=HashAttempts;
  }
  head->TableArray=(HashTable**)calloc(max_tables,sizeof(HashTable*));
  head->TableArray[0]=createTable(InitSize);
  head->cur=1;
  return head;
}

HashTable* createTable(int tsize){
  HashTable* ht=(HashTable*)calloc(1,sizeof(HashTable));
  ht->TableSize=tsize;
  ht->InnerTable=(entry**)calloc(sizeof(entry*),(ht->TableSize));
  return ht;
}
