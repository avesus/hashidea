#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>


//1048072+2095328+4076616+3767649+5883524+4784952+7124525+3552408+1197798+7432
#define ssize 32 //string size of entry/vector size
#define max_tables 64 //max tables to create
//#define vsize 15 //amount of times you want to try and hash
//#define initSize (1) //starting table size
//#define runs 1<<4
#define notIn 0 
#define in -1
#define unk -2
#define max_threads 32 //number of threads to test with
#define table_bound (1<<20)

int initSize=0;
int runs=0;
int num_threads=0;
int vsize=0;
struct h_head* global=NULL;


//items to be hashed
typedef struct int_ent{
  unsigned long val;
}int_ent;

//a table
typedef struct h_table{
  //  struct h_table* next; //next table
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
int tryAdd(int_ent* ent, unsigned int* seeds, int start);




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


uint32_t murmur_hash_64(const void * key, int len, uint64_t seed, int slots)
{
  const uint64_t m = 0xc6a4a7935bd1e995ULL;
  const int r = 47;

  uint64_t h = seed ^ (len * m);

  const uint64_t * data = (const uint64_t *)key;
  const uint64_t * end = data + (len>>3);

  while (data != end)
    {
#ifdef PLATFORM_BIG_ENDIAN
      uint64_t k = *data++;
      char *p = (char *)&k;
      char c;
      c = p[0]; p[0] = p[7]; p[7] = c;
      c = p[1]; p[1] = p[6]; p[6] = c;
      c = p[2]; p[2] = p[5]; p[5] = c;
      c = p[3]; p[3] = p[4]; p[4] = c;
#else
      uint64_t k = *data++;
#endif

      k *= m;
      k ^= k >> r;
      k *= m;

      h ^= k;
      h *= m;
    }

  const unsigned char * data2 = (const unsigned char*)data;

  switch (len & 7)
    {
    case 7: h ^= (uint64_t)(data2[6]) << 48;
    case 6: h ^= (uint64_t)(data2[5]) << 40;
    case 5: h ^= (uint64_t)(data2[4]) << 32;
    case 4: h ^= (uint64_t)(data2[3]) << 24;
    case 3: h ^= (uint64_t)(data2[2]) << 16;
    case 2: h ^= (uint64_t)(data2[1]) << 8;
    case 1: h ^= (uint64_t)(data2[0]);
      h *= m;
    };

  h ^= h >> r;
  h *= m;
  h ^= h >> r;

  return ((unsigned int)h)%slots;
  }



//string hashing function (just universal vector hash)
unsigned int hashInt(unsigned long val, int slots, hashSeeds seeds){
  unsigned long hash;
  hash=(seeds.rand1*val+seeds.rand2)%slots;
  return (unsigned int)hash;
}

//create a new table of size n
h_table* createTable(int n_size){
  h_table* ht=(h_table*)malloc(sizeof(h_table));
  ht->t_size=n_size;
  ht->s_table=(int_ent**)calloc(sizeof(int_ent*),(ht->t_size));
  //  ht->next=NULL;
  return ht;
}

void freeTable(h_table* ht){
  free(ht);
}

int max(int a, int b){
  return a*(a>b)+b*(b>=a);
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

  //unsigned int s=hashInt(ent->val, slots, seeds);
//  unsigned int s=murmur_hash_64(ent->val, 8, seeds.rand1, slots);

   unsigned int s= murmur3_32(&ent->val, 8,seeds)%slots;
  if(s_table[s]==NULL){
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
    //    ht=ht->next;
  }

  //create new table
    int new_size=0;
  if(global->tt[startCur-1]->t_size>table_bound&&startCur%2){
    new_size=global->tt[startCur-1]->t_size;
  }
  else{
    new_size=global->tt[startCur-1]->t_size<<1;
  }

  h_table* new_table=createTable(new_size);
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
      return 0;
    }
    else{
      tryAdd(ent, seeds, loc.index);
    }
  }
  return 0;
}

//print table (smallest to largest, also computes total items)

void printTables(int arr){
  FILE* fp = stdout;//fopen("output.txt","a");
  int* items=(int*)malloc(sizeof(int)*global->cur);
  h_table* ht=NULL;
  int count=0;
  for(int j=0;j<global->cur;j++){
    ht=global->tt[j];
    items[j]=0;
    //        fprintf(fp, "Table Size: %d\n", ht->t_size);
    for(int i =0;i<ht->t_size;i++){
      if(ht->s_table[i]!=NULL){
	//	fprintf(fp, "%d: %lu\n",i,ht->s_table[i]->val);
	free(ht->s_table[i]);
	items[j]++;
	count++;
      }
      else{
	//		fprintf(fp,"%d: NULL\n", i);
      }
    }
    //    fprintf(fp,"\n\n\n");
	//    ht=ht->next;
  }
  fprintf(fp,"------------------start----------------\n");
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

/*thoughts so far:
  There is nothing I don't think can be done with CAS and plenty of optimizations to make.
  The only tricky case I imagine is when resizing will have to CAS with size of tail before
  adding to it, otherwise race condition for not founds to double size at same time. This 
  means have to allocate ahead of time and if CAS fails free that memory (kind of sucks). 
  Basically everything in here is parallizable (i.e could search each table in parallel/each
  hash in parrallel so really work is logn but span is basically O(1)*/
void* run(void* argp){

  unsigned int* seeds=(unsigned int*)argp;
  for(int i =0;i<(runs);i++){
    int_ent* testAdd=(int_ent*)malloc(sizeof(int_ent));
    unsigned long temp=rand();
    testAdd->val=rand();
    testAdd->val=testAdd->val*temp;
    tryAdd(testAdd, seeds, 0);
  }
}
int main(int argc, char** argv){
  //   std::atomic<int> test;
  //   int ret=test.compare_exchange_weak(old,newv);
  //   printf("ret=%d, test=%d\n",ret, test.load());
  //initialize stuff

  if(argc!=5){
    printf("5 args\n");
    exit(0);
  }

  srand(time(NULL));
  initSize=atoi(argv[1]);
  runs=atoi(argv[2]);
  num_threads=atoi(argv[3]);
  vsize=atoi(argv[4]);
  if(num_threads>max_threads){
    num_threads=max_threads;
  }

  global=(h_head*)malloc(sizeof(h_head));
  global->tt=(h_table**)calloc(max_tables,sizeof(h_table*));
  global->cur=1;
  global->tt[0]=createTable(initSize);

  unsigned int* seeds=(unsigned int*)malloc(sizeof(unsigned int)*vsize);
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
    CPU_SET(i, &sets[i]);
    threads[i]=pthread_self();
    pthread_setaffinity_np(threads[i], sizeof(cpu_set_t),&sets[i]);
    pthread_create(&threads[i], &attr,run,(void*)seeds);


  }
  for(int i =0;i<num_threads;i++){
    pthread_join(threads[i], NULL);
    
  }


   
      printTables(0);
      free(global);
      free(seeds);

  

}

