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
#define tsize 524288
#define num_threads 32 //number of threads to test with

#define max_offset 5329500 //max offset into testing file

struct rowptr** global=NULL;
pthread_rwlock_t* rwlock=NULL; 

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
  int rand1;
  int rand2;
}hashSeeds;

void ptable();
int check(rowptr** table, int set, int val){

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
void addNode(rowptr** table, int set, int val){
  if(check(table, set, val)){
    return;
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

   
}

void ptable(){
  block* ptr=NULL;
  int amt=0;
  for(int i =0;i<tsize;i++){
    ptr=global[i]->head;
    while(ptr!=NULL){
      amt++;
      ptr=ptr->next;
      
    }

  }
  printf("amt=%d\n", amt);
}


unsigned int hashInt(unsigned long val, int slots, hashSeeds seeds){
  unsigned int hash;
  hash=(seeds.rand1*val+seeds.rand2)%slots;
  return hash;
}


void* run(void* argp){
  hashSeeds seeds=*((hashSeeds*)argp);

  //use this code instead of while(fgets..) if you want
  //to test random sample from file.
  for(int i =0;i<(1<<12);i++){

    //   while(fgets(buf, 32 ,fp)!=NULL){
    int val=rand();
    addNode(global, hashInt(val, tsize, seeds), val);
  }
}

int main(){
  srand(time(NULL));
  global=(rowptr**)malloc(tsize*sizeof(rowptr*));
  for(int i =0;i<tsize;i++){
    global[i]=(rowptr*)malloc(sizeof(rowptr));

  }
  rwlock=(pthread_rwlock_t*)malloc(tsize*sizeof(pthread_rwlock_t));
  for(int i =0;i<tsize;i++){
      pthread_rwlock_init(&rwlock[i], NULL);
  }
  hashSeeds* seeds=(hashSeeds*)malloc(sizeof(hashSeeds));
  seeds->rand1=rand();
  seeds->rand2=rand();
  int temp=rand();
  seeds->rand1=seeds->rand1*temp;
  temp=rand();
  seeds->rand2=seeds->rand2*temp;
  
  pthread_t threads[num_threads];
    pthread_attr_t attr;
  pthread_attr_init(&attr);
  cpu_set_t sets[num_threads];
  for(int i =0;i<num_threads;i++){
    CPU_ZERO(&sets[i]);
    CPU_SET(i, &sets[i]);
    threads[i]=pthread_self();
    pthread_setaffinity_np(threads[i], sizeof(cpu_set_t),&sets[i]);
    pthread_create(&threads[i], &attr,run,(void*)seeds);
  }

  for(int i =0;i<num_threads;i++){
    pthread_join(threads[i], NULL);
    
  }
    ptable();

}
