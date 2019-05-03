#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <thread>

//make: g++ -std=c++11 non-block-ll.cpp -o non-block-ll -mcx16 -lpthread -ggdb
#define max_threads 32
int num_threads=0;
int runs=0;
int initSize=1<<25;



typedef struct hashSeeds{
  unsigned long rand1;
  unsigned long rand2;
}hashSeeds;

hashSeeds seeds;

unsigned int hashInt(unsigned long val, int slots, hashSeeds seeds){
  unsigned long hash;
  hash=(seeds.rand1*val+seeds.rand2)%slots;

  return (unsigned int)hash;
}


template <typename T> class queue_t {
public:
  struct node_t;
  // Explicitly aligned due to https://gcc.gnu.org/bugzilla/show_bug.cgi?id=65147: gcc should automatically align std::atomic<pointer_t> on 16-byte boundary but doesn't (until 5.1)
  struct alignas(16) pointer_t {
    node_t* ptr;
    unsigned int count;
    // A zero-initialized pointer_t
    // I'm pretty sure we don't actually need to initialize count to 0 here given how these are used, but it can't hurt.
    pointer_t() noexcept : ptr{nullptr}, count{0} {}
    // A pointer_t pointing to a specific node
    pointer_t(node_t* ptr) : ptr{ptr}, count{0} {}
    // A pointer_t pointing to a specific node with a specific count
    pointer_t(node_t* ptr, unsigned int count) : ptr{ptr}, count{count} {}
    // bitwise-compare two pointer_ts
    bool operator ==(const pointer_t & other) const {
      return ptr == other.ptr && count == other.count;
    }
  };
  struct node_t {
    T value;
    // We're going to do atomic ops on next
    std::atomic<pointer_t> next;
    // A dummy node, next is initialized with a zero-initialized ptr
    node_t() : next{pointer_t{}} {}
    // A node filled with a given value, next is initialized with a zero-initialized ptr
    node_t(T value) : value(value), next{pointer_t{}} {}
  };

  // We're going to do atomic ops on Head
  std::atomic<pointer_t>* Head;//=new std::atomic<pointer_t>[initSize];
  // We're going to do atomic ops on Tail
  std::atomic<pointer_t>* Tail;//=new std::atomic<pointer_t>[initSize];


  queue_t(int isize) : Head{}, Tail{} {
    Head=(std::atomic<pointer_t>*)malloc(sizeof(std::atomic<pointer_t>)*initSize);//new std::atomic<pointer_t>[initSize];
    Tail=(std::atomic<pointer_t>*)malloc(sizeof(std::atomic<pointer_t>)*initSize);//new std::atomic<pointer_t>[initSize];//new std::atomic<pointer_t>[initSize];
    for(int i =0;i<isize;i++){
      Head[i]=new node_t{};
      Tail[i]=Head[i].load().ptr;
    }
  }
      //todo add forloop to init here!



  void enqueue(T value) {
    unsigned int bucket=hashInt(value, initSize, seeds);

    // Node is initialized in ctor, so three lines in one
    auto node = new node_t{value}; // E1, E2, E3
    decltype(Tail[bucket].load()) tail;
    tail=Head[bucket].load();
        while (true) { // E4
      if(!(Head[bucket].load() == Tail[bucket].load())){
	//      tail = Head.load(); // E5
	tail=tail.ptr->next;

      while((tail.ptr!=NULL)){
	if(tail.ptr->value==value){
	  return;
	}
	tail=tail.ptr->next;
      }
      
      }
	tail=Tail[bucket].load();
      // If we're the slow thread, we wait until the node we just loaded is freed.
      // Let the main thread know we're waiting
      // Use-after-free here in slow thread!

      auto next = tail.ptr->next.load(); // E6
      if (tail == Tail[bucket].load()) { // E7
	if (!next.ptr) { // E8
	  if (tail.ptr->next.compare_exchange_weak(next, pointer_t{node, next.count + 1})) { // E9
	    break; // E10
	  } // E11
	} else { // E12
	   if(next.ptr->value==value){
	    return;
	  }
	  Tail[bucket].compare_exchange_weak(tail, pointer_t{next.ptr, tail.count + 1}); // E13
	} // E14
      } // E15
    } // E16

    Tail[bucket].compare_exchange_weak(tail, pointer_t{node, tail.count + 1}); // E17
  }
};



  queue_t<unsigned long> queue{initSize};
void* run(void* argp){

  for(int i =0;i<(runs);i++){
    int ran=rand();
    unsigned long ti=rand();
    ti=ti*ran;
       queue.enqueue(ti);
  }
}




int main(int argc, char** argv){
  srand(time(NULL));
  if(argc!=3){
    printf("usage: prog runs num_threads\n");
  }
  runs=atoi(argv[1]);
  num_threads=atoi(argv[2]);
  int ran=rand();
  seeds.rand1=rand();
  seeds.rand1=seeds.rand1*ran;
  ran=rand();
  seeds.rand2=rand();
  seeds.rand2=seeds.rand2*ran;
  printf("%lu - %lu\n", seeds.rand1, seeds.rand2);

  //  int cores=sysconf(_SC_NPROCESSORS_ONLN);
  //  printf("cores=%d\n", cores);
  pthread_t threads[max_threads];
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  cpu_set_t sets[max_threads];

  for(int r =0;r<num_threads;r++){
    CPU_ZERO(&sets[r]);
    CPU_SET(r, &sets[r]);
    threads[r]=pthread_self();
    pthread_setaffinity_np(threads[r], sizeof(cpu_set_t),&sets[r]);
    pthread_create(&threads[r], &attr,run,NULL);
  }
  for(int r =0;r<num_threads;r++){
    pthread_join(threads[r], NULL);
    
  }
  int count=0;
  for(int i =0;i<initSize;i++){
  auto temp =queue.Head[i].load();
  temp=temp.ptr->next;
  //  printf("%d: ", i);
  while(temp.ptr){

    //    printf("%lu,", temp.ptr->value);
       count++;
    temp=temp.ptr->next;
  }
  //  printf("\n");
  }
  printf("count=%d\n",count);
}
