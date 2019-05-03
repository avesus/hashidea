#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#define ssize 32
//#define tsize 524288
#define max_threads 32 //number of threads to test with

#define max_offset 5329500 //max offset into testing file

int initSize=0;
int runs=0;
int num_threads=0;


struct rowptr** global=NULL;
struct rowptr** tempc=NULL; 
pthread_rwlock_t* rwlock=NULL; 
pthread_rwlock_t tcl,resizeLock;
pthread_mutex_t cl;
int new_b_size=0;
int cur_size=0;
int waitR=0;

typedef struct block{
  struct block * next;
  struct block * prev;
  unsigned long val;

}block;


typedef struct rowptr{
  block* head;
  block* tail;
}rowptr;

typedef struct hashSeeds{
  unsigned long rand1;
  unsigned long rand2;
}hashSeeds;

void ptable();
int addNode(rowptr** table, unsigned int set, unsigned long val, hashSeeds seed);
void addBlock(rowptr** curCache,block* newBlock, unsigned int set);
unsigned int hashInt(unsigned long val, int slots, hashSeeds seeds);
void removeCacheNode(int slot,block* node);
void newCache(int new_set, hashSeeds seeds);
void copyCache(rowptr** target, int new_set);
int doResize(int test);
int check(rowptr** table, unsigned int set, unsigned long val);

int check(rowptr** table, unsigned int set, unsigned long val){
  //  printf("set=%u\n", set);
  pthread_rwlock_rdlock(&rwlock[set]);
  block*ptr=table[set]->head;
  while(ptr!=NULL){
    if(ptr->val==val){
      pthread_rwlock_unlock(&rwlock[set]);
      return 1;
    }
    ptr=ptr->next;
      
  }
  pthread_rwlock_unlock(&rwlock[set]);
  return 0;

}

void addBlock(rowptr** curCache,block* newBlock, unsigned int set){
  //update a global size variable to ensure cache stays
  //below max cache size. This is only called in a writer
  //lock so no race condition.

  if(curCache[set]->head==NULL){
    curCache[set]->head=newBlock;
    curCache[set]->tail=newBlock;
    curCache[set]->head->prev=NULL;
    curCache[set]->head->next=NULL;

  }
  else{
    
    newBlock->next=curCache[set]->head;
    curCache[set]->head->prev=newBlock;
    curCache[set]->head=newBlock;
    curCache[set]->head->prev=NULL;

  }
}

void addNodeWrapped(unsigned long val, hashSeeds seeds){
  pthread_rwlock_rdlock(&tcl);
  if(tempc!=NULL){
    int ret=addNode(tempc, hashInt(val, new_b_size, seeds), val,seeds);
    if(ret){
    pthread_rwlock_unlock(&tcl);
    }
  }
  else{
    int ret=addNode(global, hashInt(val, initSize, seeds), val,seeds);
    if(ret){
    pthread_rwlock_unlock(&tcl);
    }
  }

}
int addNode(rowptr** table, unsigned int set, unsigned long val, hashSeeds seeds){

  if(check(table, set, val)){
    return 1;
  }
  pthread_rwlock_wrlock(&rwlock[set]);

  block* node=(block*)malloc(sizeof(block));
  node->val=val;
  if(table[set]->head==NULL){
    table[set]->head=node;
    table[set]->tail=node;
    node->prev=NULL;
    node->next=NULL;

  }
  else{
    node->next=table[set]->head;
    table[set]->head->prev=node;
    table[set]->head=node;
    node->prev=NULL;
  }
  pthread_rwlock_unlock(&rwlock[set]);
  int temp=0;
  pthread_mutex_lock(&cl);
  cur_size++;
  temp=cur_size;
  pthread_mutex_unlock(&cl);
    //    printf("temp = %d/%d\n", temp, initSize);
  if(temp>=initSize&&(!(temp&0x1))&&(!waitR)){
    if(doResize(temp)){
      new_b_size=temp<<1;
    pthread_rwlock_unlock(&tcl);
    newCache(temp<<1, seeds);
    return 0;
  }
  }
  return 1;
}

void ptable(){
  block* ptr=NULL;
  int amt=0;
  for(int i =0;i<initSize;i++){
    ptr=global[i]->head;
    //    printf("%d -- ", i);
    while(ptr!=NULL){
      //         printf("%lu, ", ptr->val);
      amt++;
      ptr=ptr->next;
      
    }
    //  printf("\n");

  }
  printf("amt=%d vs %d in %d buckets\n", amt, cur_size,initSize);
}


unsigned int hashInt(unsigned long val, int slots, hashSeeds seeds){
  unsigned long hash=0;

  hash=(seeds.rand1*val+seeds.rand2)%slots;
  //  printf("slot = %d, result = %u\n", slots, ((unsigned int)hash)%slots);
  return (unsigned int)hash;
}


void* run(void* argp){
  hashSeeds seeds=*((hashSeeds*)argp);

  //use this code instead of while(fgets..) if you want
  //to test random sample from file.
  for(int i =0;i<(runs);i++){

    //   while(fgets(buf, 32 ,fp)!=NULL){
    unsigned long temp=rand();
    unsigned long val=rand();
    val=val*temp;
    addNodeWrapped(val, seeds);
  }
}

void removeCacheNode(int slot,block* node){
  if(global[slot]->head==NULL){
    return;
  }
  if(node->prev==NULL&&node->next==NULL){
    global[slot]->head=NULL;
    global[slot]->tail=NULL;
  }
  else if(node->prev==NULL&&node->next!=NULL){
    global[slot]->head=node->next;
    global[slot]->head->prev=NULL;
  }
  else if(node->next==NULL&&node->prev!=NULL){
    node->prev->next=NULL;
    global[slot]->tail=node->prev;
  }
  else{
    node->prev->next=node->next;
    node->next->prev=node->prev;
  }
}


void newCache(int new_set, hashSeeds seeds){
  //malloc new cache
  rowptr** tempCache=(rowptr**)malloc(sizeof(rowptr*)*new_set);
  for(int i =0;i<new_set;i++){
    tempCache[i]=(rowptr*)malloc(sizeof(rowptr));
    tempCache[i]->head=NULL;
    tempCache[i]->tail=NULL;
  }
  pthread_rwlock_wrlock(&tcl);

  rwlock=(pthread_rwlock_t*)realloc(rwlock, new_set*(sizeof(pthread_rwlock_t)));
  for(int i =initSize;i<new_set;i++){
    pthread_rwlock_init(&rwlock[i],NULL);
    }


  tempc=tempCache;
    pthread_rwlock_unlock(&tcl);
  block* checker=NULL;
  block* checkerNext=NULL;




  //iterate through old cache starting at tail and add to new cache
  //this will mess up LRU but means that checking old cache in normal order
  //during resizing if an object is not found it will check new cache before requesting
  //info from web
  for(int i =0;i<initSize;i++){
    pthread_rwlock_wrlock(&rwlock[i]);
    checker=global[i]->head;
    while(checker!=NULL){
      checkerNext=checker->next;
      unsigned int set=hashInt(checker->val,new_set, seeds);
      if(set!=i){
	pthread_rwlock_wrlock(&rwlock[set]);
      }

      removeCacheNode(i,checker);

      addBlock(tempCache, checker, set);
      if(set!=i){
	pthread_rwlock_unlock(&rwlock[set]);
      }
      checker=checkerNext;
    }
    pthread_rwlock_unlock(&rwlock[i]);
  }

  copyCache(tempCache, new_set);
  waitR=0;
}

//free old cache ptr and set it to new  cache ptr

void copyCache(rowptr** target, int new_set){
  //lock that all other accesses to old cache go through
  //so that when deallocated never accessed. (think this was faster
  //because dont need to deal with iterating through potentially 128 rwlocks
  //and possibly be starved).
  pthread_rwlock_wrlock(&tcl);

  for(int i =0;i<initSize;i++){
    free(global[i]);
  }
  free(global);
  global=target;
  initSize=new_set;
  tempc=NULL;
  //  fprintf(stderr,"copycache2: addr- %p, %p, %p\n", global, target, tempc);
  //  fprintf(stderr,"AFTER\n");
  //  printCache(initSize);
  pthread_rwlock_unlock(&tcl);
}



int doResize(int test){
  pthread_rwlock_wrlock(&resizeLock);
  if(tempc!=NULL||waitR){
    pthread_rwlock_unlock(&resizeLock);
    return 0;
  }
  int shifter=1;
  if((initSize<<shifter)<test){
    shifter++;
  }
  if((initSize<<shifter)==test){
    waitR=1;
    pthread_rwlock_unlock(&resizeLock);
    return test;
  }
  pthread_rwlock_unlock(&resizeLock);
  return 0;
  


}

int main(int argc, char** argv){
  //  srand(time(NULL));

  if(argc!=4){
    printf("3 args\n");
    exit(0);
  }
  srand(time(NULL));
  initSize=atoi(argv[1]);
  runs=atoi(argv[2]);
  num_threads=atoi(argv[3]);
  if(num_threads>max_threads){
    num_threads=max_threads;
  }

  global=(rowptr**)malloc(initSize*sizeof(rowptr*));
  for(int i =0;i<initSize;i++){
    global[i]=(rowptr*)malloc(sizeof(rowptr));

  }
  rwlock=(pthread_rwlock_t*)malloc(initSize*sizeof(pthread_rwlock_t));
  for(int i =0;i<initSize;i++){
    pthread_rwlock_init(&rwlock[i], NULL);
  }
  pthread_rwlock_init(&tcl, NULL);
  pthread_rwlock_init(&resizeLock, NULL);

  hashSeeds* seeds=(hashSeeds*)malloc(sizeof(hashSeeds));
  seeds->rand1=rand();
  seeds->rand2=rand();
  unsigned long temp=rand();
  seeds->rand1=seeds->rand1*temp;
  temp=rand();
  seeds->rand2=seeds->rand2*temp;

  
  int cores=sysconf(_SC_NPROCESSORS_ONLN);
  pthread_t threads[max_threads];
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  cpu_set_t sets[max_threads];
  for(int i =0;i<num_threads;i++){
    CPU_ZERO(&sets[i]);
    CPU_SET(i%cores, &sets[i]);
    threads[i]=pthread_self();
    pthread_setaffinity_np(threads[i], sizeof(cpu_set_t),&sets[i]);
    pthread_create(&threads[i], &attr,run,(void*)seeds);
  }

  for(int i =0;i<num_threads;i++){
    pthread_join(threads[i], NULL);
    
  }
     ptable();

}
