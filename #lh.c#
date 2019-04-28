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
  char buf[ssize];

}block;
typedef struct rowptr{
  block* head;
  block* tail;
}rowptr;


void ptable();
int check(rowptr** table, int set, char* buf){

  pthread_rwlock_rdlock(&rwlock[set]);
  block*ptr=table[set]->head;
  while(ptr!=NULL){
    if(!strcmp(buf, ptr->buf)){
      pthread_rwlock_unlock(&rwlock[set]);
      return 1;
    }
    ptr=ptr->next;
      
  }
  pthread_rwlock_unlock(&rwlock[set]);
  return 0;

}
void addNode(rowptr** table, int set, char* buf){
  if(check(table, set, buf)){
    return;
  }
  pthread_rwlock_wrlock(&rwlock[set]);
  block* node=(block*)malloc(sizeof(block));
  strcpy(node->buf, buf);
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

unsigned long * initVec(){
  unsigned long * vec=(unsigned long*)malloc((ssize+1)*sizeof(unsigned long));
  for(int i =0;i<ssize+1;i++){
    unsigned long v_tmp=rand();
    v_tmp*=rand();
    v_tmp=((v_tmp>>1)<<1)+1;
    vec[i]=v_tmp;
  }
  return vec;
}

unsigned int hashStr(char * str, unsigned int slots, unsigned long * vec){
  unsigned long hash=vec[0];
  unsigned long UL_MAX=0;
  UL_MAX=~UL_MAX;
  int i=0;
  while(str[i]){
    hash+=(str[i]*vec[i+1])&UL_MAX;
    i++;
  }
  return hash%slots;
}


void* run(void* argp){
  unsigned long* vec=(unsigned long*)argp;
  FILE* fp=fopen("words.txt","r");
  char buf[32]="";

  //use this code instead of while(fgets..) if you want
  //to test random sample from file.
  for(int i =0;i<(runs);i++){
    fseek(fp, rand()%max_offset, SEEK_SET);
    fgets(buf, 32 ,fp);
    fgets(buf, 32 ,fp);
    //   while(fgets(buf, 32 ,fp)!=NULL){
    addNode(global, hashStr(buf, initSize, vec), buf);
  }
}

int main(int argc, char** argv){

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
  unsigned long * vec= initVec();
  

  int cores=sysconf(_SC_NPROCESSORS_ONLN);
  pthread_t threads[max_threads];
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  cpu_set_t sets[max_threads];
  for(int i =0;i<num_threads;i++){
    CPU_ZERO(&sets[i]);
    CPU_SET(i, &sets[i]);
    threads[i]=pthread_self();
    pthread_setaffinity_np(threads[i], sizeof(cpu_set_t),&sets[i]);
    pthread_create(&threads[i], &attr,run,(void*)vec);
  }

  for(int i =0;i<num_threads;i++){
    pthread_join(threads[i], NULL);
    
  }
  //    ptable();

}
