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
//#define initSize (1<<17) //starting table size
//#define runs 1<<4
#define notIn 0 
#define in -1
#define unk -2
#define max_threads 32 //number of threads to test with

int num_threads=0;
int initSize=0;
int runs=0;

#define max_offset 5329500 //max offset into testing file


struct h_head* global=NULL;


//items to be hashed
typedef struct str_ent{
  char str[ssize]; //hash computed on str
}str_ent;

//a table
typedef struct h_table{
  struct h_table* next; //next table
  str_ent** s_table; //rows (table itself)
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

int tryAdd(str_ent* ent, unsigned long ** vecs, int start);


//string hashing function (just universal vector hash)
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

//create vectors
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

//create a new table of size n
h_table* createTable(int n_size){
  h_table* ht=(h_table*)malloc(sizeof(h_table));
  ht->t_size=n_size;
  ht->s_table=(str_ent**)calloc(sizeof(str_ent*),(ht->t_size));
  ht->next=NULL;
  return ht;
}

void freeTable(h_table* ht){
  free(ht);
}

//add new table to list of tables
int addDrop(str_ent* ent, unsigned long** vecs,h_table* toadd, int tt_size){
  h_table* expected=NULL;
  int res = __atomic_compare_exchange(&global->tt[tt_size] ,&expected, &toadd, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
  if(res){
    int newSize=tt_size+1;
    __atomic_compare_exchange(&global->cur,
			      &tt_size,
			      &newSize,
			      1,__ATOMIC_RELAXED, __ATOMIC_RELAXED);
    tryAdd(ent, vecs, 1);
  }
  else{
    freeTable(toadd);
    int newSize=tt_size+1;
    __atomic_compare_exchange(&global->cur,
			      &tt_size,
			      &newSize,
			      1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
    tryAdd(ent, vecs, 1);
  }
  return 0;
}

//check if entry for a given hashing vector is in a table
int lookup(str_ent** s_table,str_ent* ent, unsigned long* vec, int slots){
  int s=hashStr(ent->str, slots, vec);
  if(s_table[s]==NULL){
    return s;
  }
  else if(!strcmp(s_table[s]->str, ent->str)){
    return in;
  }
  return unk;
}

//check if an item is found in the table, if not make new table and store it
ret_val checkTable(str_ent* ent, unsigned long** vecs, int start){
  int startCur=global->cur;
  h_table* ht=NULL;
  for(int j=start;j<global->cur;j++){
    ht=global->tt[j];
    for(int i =0;i<vsize;i++){
      int res=lookup(ht->s_table, ent, vecs[i], ht->t_size);
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
  addDrop(ent, vecs, new_table, startCur);
}

//add item using ret_val struct info
int tryAdd(str_ent* ent, unsigned long ** vecs, int start){
  ret_val loc=checkTable(ent, vecs, start);
  str_ent* expected=NULL;
  if(loc.ht){
    int res = __atomic_compare_exchange(&loc.ht->s_table[loc.slot],
					&expected,
					&ent,
					1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
    if(res){
      return 0;
    }
    else{
      tryAdd(ent, vecs, loc.index);
    }
  }
  return 0;
}

//print table (smallest to largest, also computes total items)
void printTables(int arr){
  //  FILE* fp = fopen("output.txt","wa");
  int amt=0;
  h_table* ht=NULL;
  for(int j=0;j<global->cur;j++){
    ht=global->tt[j];
    //    printf("Table %d:\n", ht->t_size);
    for(int i =0;i<ht->t_size;i++){
      if(ht->s_table[i]!=NULL){
	amt++;
	//	fprintf(fp, "%s",ht->s_table[i]->str);
	//	printf("%d: %s\n", i, ht->s_table[i]->str);
      }
      else{
	//	printf("%d: NULL\n", i);
      }
    }
    //    printf("\n\n\n");
    ht=ht->next;
  }
  printf("amt=%d\n", amt);
}

/*thoughts so far:
  There is nothing I don't think can be done with CAS and plenty of optimizations to make.
  The only tricky case I imagine is when resizing will have to CAS with size of tail before
  adding to it, otherwise race condition for not founds to double size at same time. This 
  means have to allocate ahead of time and if CAS fails free that memory (kind of sucks). 
  Basically everything in here is parallizable (i.e could search each table in parallel/each
  hash in parrallel so really work is logn but span is basically O(1)*/
void* run(void* argp){

  unsigned long** vecs=(unsigned long**)argp;
  FILE* fp=fopen("words.txt","r");
  char buf[32]="";

  //use this code instead of while(fgets..) if you want
  //to test random sample from file.
  for(int i =0;i<(runs);i++){
    fseek(fp, rand()%max_offset, SEEK_SET);
    fgets(buf, 32 ,fp);
    fgets(buf, 32 ,fp);
    //   while(fgets(buf, 32 ,fp)!=NULL){
    str_ent* testAdd=(str_ent*)malloc(sizeof(str_ent));
    strcpy(testAdd->str, buf);
    tryAdd(testAdd, vecs, 0);
  }


}
int main(int argc, char** argv){
  //   std::atomic<int> test;
  //   int ret=test.compare_exchange_weak(old,newv);
  //   printf("ret=%d, test=%d\n",ret, test.load());
  //initialize stuff
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

  global=(h_head*)malloc(sizeof(h_head));
  global->tt=(h_table**)calloc(32,sizeof(h_table*));
  global->cur=1;
  global->tt[0]=createTable(initSize);

  unsigned long ** vecs=(unsigned long**)malloc(sizeof(unsigned long*)*vsize);
  for(int i =0;i<vsize;i++){
    vecs[i]=initVec();
  }

  
  //using word file with ~500k words for testing
  FILE* fp=fopen("words.txt","r");
  char buf[32]="";

  //creating num_threads threads to add items in parallel
  //see run function for more details
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
    pthread_create(&threads[i], &attr,run,(void*)vecs);


  }
  for(int i =0;i<num_threads;i++){
    pthread_join(threads[i], NULL);
    
  }


   
  /*  printTables(0);
      printf("end sizes: ");
      h_table* temp=NULL;
      for(int i =0;i<global->cur;i++){
      temp=global->tt[i];
      printf("%d - ", temp->t_size);
      temp=temp->next;
      }
      printf("\n");*/
  printf("size = %d\n", global->tt[global->cur-1]->t_size);

  
  

}

