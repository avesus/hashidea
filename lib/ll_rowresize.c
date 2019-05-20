#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include "hash.h"

//make: gcc ms.c -o ms -lpthread -latomic
int num_threads=0;
int runs=0;
int initSize=0;
int max_ele=0;

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
}table;

typedef struct g_head{
  table** t;
  int cur;

}g_head;

int getAmt(volatile node* ptr){
  return ((unsigned long)ptr)&0xf;
}
volatile node* getPtr(volatile node* ptr){
  unsigned long mask=0xf;
  return (volatile node*)((((unsigned long)ptr)&(~mask)));
}
void setPtr(volatile node** ptr, int amt){

  *ptr=(void*)(((unsigned long)getPtr(*ptr))|amt);
}
void incPtr(volatile node** ptr){

  int amt=getAmt(*ptr)+1;
  *ptr=(void*)(((unsigned long)getPtr(*ptr))|amt);
}
void enq(node* new_node);
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
uint32_t murmur3_32(const uint8_t* key, size_t len, uint32_t seed);
int searchq(que** q, unsigned long val){
  //  printf("start search\n");
  unsigned int bucket= murmur3_32(&val, 8, seeds)%initSize;
  volatile pointer tail=q[bucket]->head;
  while(getPtr(tail.ptr)!=NULL){
    if(getPtr(tail.ptr)->val==val){
      //  printf("end search\n");
      return 1;
    }
    tail=getPtr(tail.ptr)->next;
  }
  //  printf("end search\n");
  return 0;
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
    enq(ele);
  }
  else{
    freeTable(toadd);
    int newSize=tt_size+1;
    __atomic_compare_exchange(&global->cur,
			      &tt_size,
			      &newSize,
			      1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
    enq(ele);
  }
  return 0;
}


volatile pointer makeFrom(volatile node* new_node, int amt){
  volatile pointer p = {.ptr=new_node};
  setPtr(&(p.ptr),amt);
  return p;
}



void enq(node* new_node){
  int startCur=global->cur;
  table* t;
  for(int i =0;i<global->cur;i++){
    t=global->t[i];
    unsigned int bucket= murmur3_32(&new_node->val, 8, seeds)%t->size;

    volatile pointer tail;
    volatile pointer next;
    tail=t->q[bucket]->head;

    int should_hit=0;
    while(1){
    
      while(getPtr(tail.ptr)!=getPtr(t->q[bucket]->tail.ptr)){
	if(getPtr(tail.ptr)->val==getPtr(new_node)->val){
	  return;
	}
	tail=getPtr(tail.ptr)->next;
      }
      next=getPtr(tail.ptr)->next;
      if(getPtr(tail.ptr)->val==getPtr(new_node)->val){
	return;
      }


      if(getPtr(tail.ptr)==getPtr(t->q[bucket]->tail.ptr)){
	if(getPtr(next.ptr)==NULL){
       
	  int pval=getAmt(tail.ptr)+1;
	  // printf("pval=%d\n",pval);
	  if(pval>=max_ele){
	    break;
	  }
	  volatile pointer p=makeFrom(new_node, pval);
	  if(__atomic_compare_exchange((pointer*)&(getPtr(tail.ptr)->next) ,(pointer*)&next,(pointer*) &p, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)){
	    should_hit=1;
	    break;
	  }
	  else{
	    if(next.ptr->val==new_node->val){
	      return;
	    }
	    int pval=getAmt(next.ptr);
	    //	         printf("pval=%d\n",pval);
	    /*	  if(pval>=max_ele){
		  if(searchq(t->q, getPtr(new_node)->val)){
		  return;
		  }
		  break;
		  }*/
	    volatile pointer p2=makeFrom(next.ptr, pval);
	    if(__atomic_compare_exchange((pointer*)&t->q[bucket]->tail ,(pointer*)&tail, (pointer*)&p2, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)){
	    }
	  }
	}
      }
    }
    if(should_hit){
      int pval=getAmt(t->q[bucket]->tail.ptr)+1;
      //    printf("pval=%d\n",pval);
      volatile pointer p3=makeFrom(new_node, pval);
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
  int r_amt=0;
  for(int j =0;j<global->cur;j++){
    t=global->t[j];
            printf("Table %d Size %d\n", j, t->size);
    extra+=t->size;
 for(int i =0;i<t->size;i++){
   volatile node* temp=getPtr(t->q[i]->head.ptr);
  if(todo)
    printf("%d: ", i);
  int pval=getAmt(temp->next.ptr);
  temp=getPtr(temp->next.ptr);

  while(temp!=NULL){

    amt++;
    r_amt++;

      if(todo)
	printf("%p - (%d) %lu, ", getPtr(temp),pval,temp->val);
      if(pval!=r_amt||pval>=max_ele){
		printf("fuck up\n");
      }
      
      pval=getAmt(temp->next.ptr);
      temp=getPtr(temp->next.ptr);

  }
  r_amt=0;
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
  for(int i =0;i<(runs);i++){
        unsigned long val=rand();
        val=val*rand();
	node* new_node = (node*)malloc(sizeof(node));
	
	new_node->next.ptr=NULL;
	new_node->val=i;
	enq(new_node);
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


    printTable(0);

}
