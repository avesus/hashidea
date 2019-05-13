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
int max_ele=1;
pthread_mutex_t cm; //various locks
int barrier=0;
unsigned int seeds=0;
struct g_head* global=NULL;
#define max_threads 32
#define max_tables 64
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

typedef struct table{
  que ** q;
  int size;
  int items;
}table;

typedef struct g_head{
  table** t;
  int cur;

}g_head;


void enq(node* new_node, int start, int b);
void freeTable(table* toadd){
  for(int i =0;i<toadd->size;i++){
    free(toadd->q[i]);
  }
  free(toadd->q);
  free(toadd);

}
table* createTable(int n_size){
  table* t=(table*)malloc(sizeof(table));
  t->size=n_size;
  t->items=0;
  t->q=(que**)malloc(sizeof(que*)*(t->size));
  //  ht->next=NULL;
  for(int i =0;i<n_size;i++){
  node* new_node = (node*)malloc(sizeof(node));
    new_node->val=-1;
  t->q[i]=(que*)malloc(sizeof(que));
  new_node->next.ptr=NULL;
  t->q[i]->head.ptr=new_node;
  t->q[i]->tail.ptr=new_node;
  }

  return t;
}



int addDrop(node* ele ,table* toadd, int tt_size){
  table* expected=NULL;
  int res = __atomic_compare_exchange(&global->t[tt_size] ,&expected, &toadd, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
  if(res){
    int newSize=tt_size+1;
    __atomic_compare_exchange(&global->cur,
			      &tt_size,
			      &newSize,
			      1,__ATOMIC_RELAXED, __ATOMIC_RELAXED);
    enq(ele,0,-1);
  }
  else{
    freeTable(toadd);
    int newSize=tt_size+1;
    __atomic_compare_exchange(&global->cur,
			      &tt_size,
			      &newSize,
			      1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
    enq(ele,0,-1);
  }
  return 0;
}



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



volatile pointer makeFrom(volatile node* new_node){
  volatile pointer p = {.ptr=new_node};
  return p;
}

void enq(node* new_node, int start, int b){
  //  printf("adding %lu\n", new_node->val);
  int startCur=global->cur;
  table* t;
  for(int i =start;i<global->cur;i++){
    t=global->t[i];
    unsigned int bucket;
    if(b>=0){
      bucket=b;
    }
  else{
    bucket= murmur3_32(&new_node->val, 8, seeds)%t->size;
  }
  volatile pointer tail;
  volatile pointer next;
  tail=t->q[bucket]->head;
  int should_hit=0;
  while(1){

  while(tail.ptr!=t->q[bucket]->tail.ptr){

    if(tail.ptr->val==new_node->val){
      return;
    }
    tail=tail.ptr->next;
  }

 next=tail.ptr->next;
 if(tail.ptr->val==new_node->val){
   return;
 }
 if(!t->q[bucket]->tail.ptr->val){
   if(new_node->val){
     //printf("next at %lu in %d\n", new_node->val, i);
   break;
   }
   else{
     return;
   }
 }
 if(t->items>(max_ele*t->size)&&new_node->val){
   //   printf("here at %lu in %d\n", new_node->val, i);
   node* end_node = (node*)malloc(sizeof(node));
   end_node->next.ptr=NULL;
   end_node->val=0;
   enq(end_node, i, bucket);
 }
 // printf("add at %lu in %d\n", new_node->val, i);

    if(tail.ptr==t->q[bucket]->tail.ptr){
      if(next.ptr==NULL){
	volatile pointer p=makeFrom(new_node);
	if(__atomic_compare_exchange((pointer*)&tail.ptr->next ,(pointer*)&next,(pointer*) &p, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)){
	  should_hit=1;
	  t->items++;	  
	  break;
	}
	else{
	  if(next.ptr->val==new_node->val){
	    return;
	  }


	  volatile pointer p2=makeFrom(next.ptr);
	  if(__atomic_compare_exchange((pointer*)&t->q[bucket]->tail ,(pointer*)&tail, (pointer*)&p2, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)){
	  }
	}
      }
    }
  }
  if(should_hit){
  volatile pointer p3=makeFrom(new_node);
  if(__atomic_compare_exchange((pointer*)&t->q[bucket]->tail ,(pointer*)&tail, (pointer*)&p3, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)){
  }
  return;
  }
  startCur=global->cur;
  }
  table* new_table=createTable(global->t[startCur-1]->size<<1);
  addDrop(new_node, new_table, startCur);
}
void printTable(int todo){

  int amt=0;
  table* t;
  int extra=0;
  for(int j =0;j<global->cur;j++){
    t=global->t[j];
    printf("Table %d Size %d vs %d\n", j, t->size, t->items);
    extra+=t->size;
 for(int i =0;i<t->size;i++){
  volatile node* temp=t->q[i]->head.ptr;
  if(todo)
    printf("%d: ", i);
  temp=temp->next.ptr;
  while(temp!=NULL){
      if(temp->val!=0){
        amt++;
	    }
      if(todo)
	printf("%lu, ", temp->val);
  temp=temp->next.ptr;

  }
  if(todo)
    printf("\n");
 }
   if(todo)
     printf("\n\n\n");
  }
  printf("amt=%d\n", amt);


}
void* run(void* argp){
  //que** q=(que**)argp;
    pthread_mutex_lock(&cm);
  barrier++;
  pthread_mutex_unlock(&cm);
  while(barrier<num_threads){

  }


  for(int i =1;i<=(runs);i++){
        unsigned long val=rand();
        val=val*rand();
	node* new_node = (node*)malloc(sizeof(node));
	
	new_node->next.ptr=NULL;
	new_node->val=i;
	enq(new_node, 0,-1);
  }
}

int main(int argc, char**argv){
  if(argc!=5){
    printf("usage: prog initsize runs num_threads\n");
  }
  initSize=atoi(argv[1]);
  runs=atoi(argv[2]);
  num_threads=atoi(argv[3]);
  max_ele=atoi(argv[4]);
  global=(g_head*)malloc(sizeof(g_head));
  global->t=(table**)malloc(max_tables*sizeof(table*));
  global->cur=1;
  global->t[0]=createTable(initSize);
  //  que** q=(que**)malloc(sizeof(que*)*initSize);
  //  start(q, initSize);

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
    pthread_create(&threads[i], &attr,run,NULL);


  }
  for(int i =0;i<num_threads;i++){
    pthread_join(threads[i], NULL);
    
  }


    printTable(0)  ;

}
