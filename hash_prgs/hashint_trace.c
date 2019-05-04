#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

//todo: check if fail cas, volatile types


#define max_tables 64 //max tables to create

//constants for returns of lookup fun
#define notIn 0 
#define in -1
#define unk -2


#define max_threads 32 //number of threads to test with

//size of struct (for murmur_hash)
#define d_size  sizeof(struct int_ent)

//globals defining what to do
int initSize=0;
int runs=0;
int num_threads=0;
int vsize=0;
char path[32]="";

//pointer to table
struct h_head* global=NULL;


//items to be hashed
typedef struct int_ent{
  unsigned long val;
}int_ent;

//a table
typedef struct h_table{
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

typedef struct t_args{
  unsigned int * seeds;
  int t_num;
}t_args;

int tryAdd(int_ent* ent, unsigned int* seeds, int start);


//murmur hash, a little research made me think this is best one to use
//this implimentation copied from wikipedia
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






//exact same as lookup but different returns
int lookupQuery(int_ent** s_table,int_ent*ent, unsigned int seeds, int slots){
  unsigned int s=murmur3_32(&ent->val, d_size, seeds)%slots;
  if(s_table[s]==NULL){
    return notIn;
  }
  else if(ent->val==s_table[s]->val){
    return in;
  }
  return unk;
}


//basically exact same logic as checktable but different returns
int checkTableQuery(int_ent* ent, unsigned int* seeds){
  
  h_table* ht=NULL;
  for(int j=0;j<global->cur;j++){
    ht=global->tt[j];
    for(int i =0;i<vsize;i++){
      int res=lookupQuery(ht->s_table, ent, seeds[i], ht->t_size);
      if(res==unk){ //unkown if in our not
	continue;
      }
      if(res==notIn){ //not in (we got null)
	return 0;
      }
      //is in 
      
      return 1;
    }
  }
  return 0;
}



//create a new table of size n
h_table* createTable(int n_size){
  h_table* ht=(h_table*)malloc(sizeof(h_table));
  ht->t_size=n_size;
  ht->s_table=(int_ent**)calloc(sizeof(int_ent*),(ht->t_size));
  //  ht->next=NULL;
  return ht;
}

//very complicated function here
void freeTable(h_table* ht){
  free(ht);
}


//add new table to list of tables
int addDrop(int_ent* ent, unsigned int* seeds,h_table* toadd, int tt_size){
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
int lookup(int_ent** s_table,int_ent* ent, unsigned int seeds, int slots){
  unsigned int s= murmur3_32(&ent->val, d_size, seeds)%slots;
  if(s_table[s]==NULL){
    //nothing there so return slot so can try and CAS
    return s;
  }
  else if(s_table[s]->val==ent->val){
    return in;
  }
  return unk;
}

//check if an item is found in the table, if not make new table and store it
ret_val checkTable(int_ent* ent, unsigned int* seeds, int start){
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
  }

  //create new table
  h_table* new_table=createTable(global->tt[startCur-1]->t_size<<1);
  addDrop(ent, seeds, new_table, startCur);
}

//add item using ret_val struct info
int tryAdd(int_ent* ent, unsigned int* seeds, int start){
  ret_val loc=checkTable(ent, seeds, start);
  int_ent* expected=NULL;
  if(loc.ht){
    int res = __atomic_compare_exchange(&loc.ht->s_table[loc.slot],
					&expected,
					&ent,
					1, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
    if(res){
      //if succesful CAS return, else try again starting at index
      return 0;
    }
    else{
      tryAdd(ent, seeds, loc.index);
    }
  }
  return 0;
}

//print table (smallest to largest, also computes total items)
//and some other useful information
void printTables(unsigned int* seeds){
  FILE* fp = fopen("output.txt","a"); //outputs to file, change fp to whatever you want
  int* items=(int*)malloc(sizeof(int)*global->cur);
  h_table* ht=NULL;
  int count=0;
  for(int j=0;j<global->cur;j++){
    ht=global->tt[j];
    items[j]=0;
    //uncomment these prints if you want to see exact output (dont do this for big files)

    //          fprintf(fp, "Table Size: %d\n", ht->t_size);
    for(int i =0;i<ht->t_size;i++){
      if(ht->s_table[i]!=NULL){
	//       	fprintf(fp, "%d: %lu\n",i,ht->s_table[i]->val); 
	free(ht->s_table[i]);
	items[j]++;
	count++;
      }
      else{
	//		fprintf(fp,"%d: NULL\n", i);
      }
    }
    //       fprintf(fp,"\n\n\n");
  }

  //like report I find useful
  fprintf(fp,"------------------start %d %d----------------\n", initSize, vsize);
  fprintf(fp,"count=%d\n", count);

  h_table* temp=NULL;
  fprintf(fp,"tables:\n");
  for(int j=0;j<global->cur;j++){
    temp=global->tt[j];
    fprintf(fp,"%d/%d\n", items[j],temp->t_size);
    free(temp);
  }
  fprintf(fp,"------------------end----------------\n");
}


//each threas function
void* run(void* argp){

  t_args* args = (t_args*)argp;

  unsigned int* seeds=args->seeds;
  int t_num=args->t_num;
  free(args);
  char local_path[64]="";

  //getting its own trace file (p<num>.txt)
  sprintf(local_path,"%sp%d.txt", path, t_num);
  FILE* fp=fopen(local_path, "r");
  char buf[64]="";

  //running
  while(fgets(buf, 64, fp)!=NULL){

    //case to insert an item
    if(buf[0]=='A'){
      char* end;
      int_ent* testAdd=(int_ent*)malloc(sizeof(int_ent));
      testAdd->val=strtoull(buf+2,&end, 10);
      tryAdd(testAdd, seeds, 0);
    }

    //case to query (high chance its in there)
    else if(buf[0]=='T'){
      char* end;
      int_ent* testAdd=(int_ent*)malloc(sizeof(int_ent));
      testAdd->val=strtoull(buf+2,&end, 10);
      checkTableQuery(testAdd, seeds);
    }

    //case2 to query (very lower chance its in there if using ULL)
    else if(buf[0]=='Q'){
      char* end;
      int_ent* testAdd=(int_ent*)malloc(sizeof(int_ent));
      testAdd->val=strtoull(buf+2,&end, 10);
      checkTableQuery(testAdd, seeds);
    }
    else{

      //if you see this something is wrong
      printf("bad file %s\n", buf);
    }
  }
  fclose(fp);
}
int main(int argc, char** argv){

  //check args
  if(argc!=5){
    printf("5 args\n");
    exit(0);
  }

  //srand each time for new seeds etc..
  srand(time(NULL));

  //initialize values from usage input
  initSize=atoi(argv[1]);
  num_threads=atoi(argv[2]);
  vsize=atoi(argv[3]);
  strcpy(path, argv[4]);
  if(num_threads>max_threads){
    num_threads=max_threads;
  }

  //initialize first table
  global=(h_head*)malloc(sizeof(h_head));
  global->tt=(h_table**)calloc(max_tables,sizeof(h_table*));
  global->cur=1;
  global->tt[0]=createTable(initSize);

  //create hash seeds
  unsigned int * seeds=(unsigned int*)malloc(sizeof(unsigned int)*vsize);
  for(int i =0;i<vsize;i++){
    seeds[i]=rand();
  }

  
  //creating num_threads threads to add items in parallel
  //see run function for more details
  int cores=sysconf(_SC_NPROCESSORS_ONLN);
  pthread_t threads[max_threads];
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  cpu_set_t sets[max_threads];
  for(int i =0;i<num_threads;i++){
    CPU_ZERO(&sets[i]);
    CPU_SET(i, &sets[i]); //mode i by num cores if you are doing more threads than cores
    threads[i]=pthread_self();
    pthread_setaffinity_np(threads[i], sizeof(cpu_set_t),&sets[i]);
    t_args* temp=(t_args*)malloc(sizeof(t_args));
    temp->seeds=seeds;
    temp->t_num=i;
    pthread_create(&threads[i], &attr,run,(void*)temp);
  }
  for(int i =0;i<num_threads;i++){
    pthread_join(threads[i], NULL);
  }


  //comment these out for performance testing   
  printTables(seeds);
  free(global);
  free(seeds);

  

}

