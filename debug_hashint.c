#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#define ssize 32 //string size of entry/vector size
#define vsize 3 //amount of times you want to try and hash
#define initSize (1) //starting table size
#define notIn 0 
#define in -1
#define unk -2
#define num_threads 32 //number of threads to test with
#define test_num 100

pthread_mutex_t cm; //various locks
int barrier=0;
struct h_head* global=NULL;


//items to be hashed
typedef struct int_ent{
  unsigned long val;
}int_ent;

//a table
typedef struct h_table{
  struct h_table* next; //next table
  int_ent** s_table; //rows (table itself)
  int t_size; //size
}h_table;

//return value for a match, can look up item in slot of table
typedef struct ret_val{
  h_table* ht;
  int slot;
  int index;
}ret_val;

//head of cache
typedef struct h_head{
  h_table** tt; //array of tables
  int cur; //current max index (max exclusive)
}h_head;

typedef struct hashSeeds{
  unsigned long rand1;
  unsigned long rand2;
}hashSeeds;
int tryAdd(int_ent* ent, hashSeeds* seeds, int start);


//string hashing function (just universal vector hash)
unsigned int hashInt(unsigned long val, int slots, hashSeeds seeds){
  unsigned long hash;
  hash=(seeds.rand1*val+seeds.rand2)%slots;
  return (int)hash;
}

//create a new table of size n
h_table* createTable(int n_size){
  h_table* ht=(h_table*)malloc(sizeof(h_table));
  ht->t_size=n_size;
  ht->s_table=(int_ent**)calloc(sizeof(int_ent*),(ht->t_size));
  ht->next=NULL;
  return ht;
}

void freeTable(h_table* ht){
  free(ht);
}

int max(int a, int b){
  return a*(a>b)+b*(b>=a);
}
//add new table to list of tables
int addDrop(int_ent* ent, hashSeeds* seeds,h_table* toadd, int tt_size){
  h_table* expected=NULL;
  int res = __atomic_compare_exchange(&global->tt[tt_size] ,&expected, &toadd, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
  if(res){
    int newSize=tt_size+1;
    __atomic_compare_exchange(&global->cur,
			      &tt_size,
			      &newSize,
			      1,__ATOMIC_RELAXED, __ATOMIC_RELAXED);
    tryAdd(ent, seeds,1);
  }
  else{
    freeTable(toadd);
    int newSize=tt_size+1;
    __atomic_compare_exchange(&global->cur,
			      &tt_size,
			      &newSize,
			      1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
    tryAdd(ent, seeds, 1);
  }
  return 0;
}

//check if entry for a given hashing vector is in a table
int lookup(int_ent** s_table,int_ent* ent, hashSeeds seeds, int slots){

  int s=hashInt(ent->val, slots, seeds);
  if(s_table[s]==NULL){
    return s;
  }
  else if(s_table[s]->val==ent->val){
    return in;
  }
  return unk;
}

//check if an item is found in the table, if not make new table and store it
ret_val checkTable(int_ent* ent, hashSeeds* seeds, int start){
  int startCur=global->cur;
  h_table* ht=NULL;
  for(int j=start;j<global->cur;j++){
    ht=global->tt[j];
    for(int i =0;i<vsize;i++){
      int res=lookup(ht->s_table, ent, seeds[i], ht->t_size);
      if(res==unk){ //unkown if in our not
	continue;
      }
      if(res==in){ //is in
	ret_val ret={ .ht=NULL, .slot=0, .index=j};
	return ret;
      }
      //not in (we got null so it would have been there)
      ret_val ret={ .ht=ht, .slot=res, .index=j};
      return ret;
    }
    startCur=global->cur;
    ht=ht->next;
  }

  //create new table
  h_table* new_table=createTable(global->tt[startCur-1]->t_size<<1);
  addDrop(ent, seeds, new_table, startCur);
}

//add item using ret_val struct info
int tryAdd(int_ent* ent, hashSeeds* seeds, int start){
  ret_val loc=checkTable(ent, seeds, start);
  int_ent* expected=NULL;
  if(loc.ht){
    int res = __atomic_compare_exchange(&loc.ht->s_table[loc.slot],
					&expected,
					&ent,
					1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
    if(res){
      return 0;
    }
    else{
      tryAdd(ent, seeds, loc.index);
    }
  }
  return 0;
}

//print table (smallest to largest, also computes total items)
int count(){
 int amt=0;
  h_table* ht=NULL;
  for(int j=0;j<global->cur;j++){
    ht=global->tt[j];
    for(int i =0;i<ht->t_size;i++){
      if(ht->s_table[i]!=NULL){
	amt++;
      }
      else{
	//	printf("%d: NULL\n", i);
      }
    }
    //    printf("\n\n\n");
    ht=ht->next;
  }
  return amt;
}
void printTables(int arr){
 FILE* fp = fopen("output.txt","wa");
 FILE* fpd=NULL;
 int amt=count();
 fprintf(fp, "%d\n",amt);
 int debug=0;
 if(amt!=test_num){
   debug=1;
   fpd=fopen("debug.txt","a");
 }
  h_table* ht=NULL;
  for(int j=0;j<global->cur;j++){
    ht=global->tt[j];
    if(debug){
      fprintf(fpd, "Table Size: %d\n", ht->t_size);
    }
    for(int i =0;i<ht->t_size;i++){
      if(ht->s_table[i]!=NULL){

	fprintf(fp, "%lu\n",ht->s_table[i]->val);
	if(debug){
	  fprintf(fpd, "%d: %lu\n",i,ht->s_table[i]->val);
	}
      }
      else{
	if(debug){
	  fprintf(fpd,"%d: NULL\n", i);
	}
      }
    }
    if(debug){
      fprintf(fpd,"\n\n\n");
    }
    ht=ht->next;
  }
  if(debug){
   fprintf(fpd,"\n\n\n");
   fprintf(fpd,"-----------------------------------------------");
   fprintf(fpd,"\n\n\n");

  }
}

/*thoughts so far:
  There is nothing I don't think can be done with CAS and plenty of optimizations to make.
  The only tricky case I imagine is when resizing will have to CAS with size of tail before
  adding to it, otherwise race condition for not founds to double size at same time. This 
  means have to allocate ahead of time and if CAS fails free that memory (kind of sucks). 
  Basically everything in here is parallizable (i.e could search each table in parallel/each
  hash in parrallel so really work is logn but span is basically O(1)*/
void* run(void* argp){

  hashSeeds* seeds=(hashSeeds*)argp;
  FILE* fp=fopen("words.txt","r");
  char buf[32]="";
  pthread_mutex_lock(&cm);
  barrier++;
  pthread_mutex_unlock(&cm);
  while(barrier<num_threads){

  }

  for(int i =0;i<(1<<10);i++){

    int_ent* testAdd=(int_ent*)malloc(sizeof(int_ent));
    testAdd->val=rand()%test_num;
    tryAdd(testAdd, seeds, 0);
  }


}
int main(){
  //   std::atomic<int> test;
  //   int ret=test.compare_exchange_weak(old,newv);
  //   printf("ret=%d, test=%d\n",ret, test.load());
  //initialize stuff
  srand(time(NULL));

  global=(h_head*)malloc(sizeof(h_head));
  global->tt=(h_table**)calloc(32,sizeof(h_table*));
  global->cur=1;
  global->tt[0]=createTable(initSize);

  hashSeeds* seeds=(hashSeeds*)malloc(sizeof(hashSeeds)*vsize);
  for(int i =0;i<vsize;i++){
    seeds[i].rand1=rand();
    seeds[i].rand2=rand();
    int temp=rand();
    seeds[i].rand1=seeds[i].rand1*temp;
    temp=rand();
    seeds[i].rand2=seeds[i].rand2*temp;

  }

  
  //creating num_threads threads to add items in parallel
  //see run function for more details
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


   
   printTables(0);
   FILE * fps =fopen("results.txt", "a");
   fprintf(fps,"-------------------- START CHECK -----------------\n");
   fprintf(fps,"end sizes: ");
  h_table* temp=NULL;
  for(int i =0;i<global->cur;i++){
    temp=global->tt[i];
    fprintf(fps, "%d - ", temp->t_size);
    temp=temp->next;
  }
  fprintf(fps,"\n");

  
  

}

