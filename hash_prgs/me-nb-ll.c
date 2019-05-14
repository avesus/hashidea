#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>

//make: gcc ms.c -o ms -lpthread -latomic
int num_threads=0;
int runs=0;
int initSize=0;
pthread_mutex_t cm; //various locks
int barrier=0;
unsigned int seeds=0;

#define max_threads 32
typedef struct pointer{
  volatile struct node* ptr;
  //if want deletion gotta add count var here (need to update atomic to 16 byte version then) 
}pointer;

typedef struct node{
  volatile unsigned long val;
  volatile struct pointer next;
}node;

typedef struct que{
  volatile struct pointer head;
  volatile struct pointer tail;
}que;
  


uint32_t murmur3_32(const uint8_t* key, size_t len, uint32_t seed)
{
  uint32_t h = seed;
  if (len > 3) {
    const uint32_t* key_x4 = (const uint32_t*) key;
    size_t i = len >> 2;
    do {
      uint32_t k = *key_x4++;
      k *= 0xcc9e2d51;
      k = (k << 15) | (k >> 17);
      k *= 0x1b873593;
      h ^= k;
      h = (h << 13) | (h >> 19);
      h = h * 5 + 0xe6546b64;
    } while (--i);
    key = (const uint8_t*) key_x4;
  }
  if (len & 3) {
    size_t i = len & 3;
    uint32_t k = 0;
    key = &key[i - 1];
    do {
      k <<= 8;
      k |= *key--;
    } while (--i);
    k *= 0xcc9e2d51;
    k = (k << 15) | (k >> 17);
    k *= 0x1b873593;
    h ^= k;
  }
  h ^= len;
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;
  return h;
}


void start(que** q, int size){
  for(int i =0;i<size;i++){
  node* new_node = (node*)malloc(sizeof(node));
  q[i]=(que*)malloc(sizeof(que));
  new_node->next.ptr=NULL;
  q[i]->head.ptr=new_node;
  q[i]->tail.ptr=new_node;
  }

}

volatile pointer makeFrom(volatile node* new_node){
  volatile pointer p = {.ptr=new_node};
  return p;
}

void enq(que** q, unsigned long val){
  unsigned int bucket= murmur3_32(&val, 8, seeds)%initSize;
  node* new_node = (node*)malloc(sizeof(node));
  new_node->next.ptr=NULL;
  new_node->val=val;
  volatile pointer tail;
  volatile pointer next;
  tail=q[bucket]->head;
  while(1){

  while(tail.ptr!=q[bucket]->tail.ptr){

    if(tail.ptr->val==val){
      return;
    }
    tail=tail.ptr->next;
  }
 next=tail.ptr->next;
 if(tail.ptr->val==val){
   return;
 }
    if(tail.ptr==q[bucket]->tail.ptr){
      if(next.ptr==NULL){
	volatile pointer p=makeFrom(new_node);
	if(__atomic_compare_exchange((pointer*)&tail.ptr->next ,(pointer*)&next,(pointer*) &p, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)){
	  break;
	}
	else{
	  if(next.ptr->val==val){
	    return;
	  }
	  volatile pointer p2=makeFrom(next.ptr);
	  if(__atomic_compare_exchange((pointer*)&q[bucket]->tail ,(pointer*)&tail, (pointer*)&p2, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)){
	  }
	}
      }
    }
  }
  volatile pointer p3=makeFrom(new_node);
  if(__atomic_compare_exchange((pointer*)&q[bucket]->tail ,(pointer*)&tail, (pointer*)&p3, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)){
  }
}

void* run(void* argp){
  que** q=(que**)argp;
    pthread_mutex_lock(&cm);
  barrier++;
  pthread_mutex_unlock(&cm);
  while(barrier<num_threads){

  }


  for(int i =1;i<=(runs);i++){
        unsigned long val=rand();
        val=val*rand();
    enq(q, i);
  }
}

int main(int argc, char**argv){
  if(argc!=4){
    printf("usage: prog initsize runs num_threads\n");
  }
  initSize=atoi(argv[1]);
  runs=atoi(argv[2]);
  num_threads=atoi(argv[3]);
  que** q=(que**)malloc(sizeof(que*)*initSize);
  start(q, initSize);

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
    pthread_create(&threads[i], &attr,run,(void*)q);


  }
  for(int i =0;i<num_threads;i++){
    pthread_join(threads[i], NULL);
    
  }



    int amt=0;
  for(int i =0;i<initSize;i++){
  volatile node* temp=q[i]->head.ptr;

  while(temp!=NULL){
    amt++;
    //    printf("%d: %lu\n", i, temp->val);
    temp=temp->next.ptr;

  }
  }
  printf("amt=%d\n", amt-initSize);
}

