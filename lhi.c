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
  for(int i =0;i<initSize;i++){
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
  for(int i =0;i<(runs);i++){

    //   while(fgets(buf, 32 ,fp)!=NULL){
    unsigned long temp=rand();
    unsigned long val=rand();
    val=val*temp;
    addNode(global, hashInt(val, initSize, seeds), val);
  }
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
  hashSeeds* seeds=(hashSeeds*)malloc(sizeof(hashSeeds));
  seeds->rand1=rand();
  seeds->rand2=rand();
  int temp=rand();
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
  //    ptable();

}
