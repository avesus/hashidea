#define _GNU_SOURCE
#include <sched.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#define size (1<<27)
#define lineSize 64
#define offset 16
#define myMod (size)
#define mp 50
#define MIN(X,Y)   ((X) < (Y) ? (X) : (Y))

void mStride(int * arr, int stride){

  volatile int sink;
  int sum;
  for(int i=0;i<(size);i+=stride){
    sum+=arr[i];
   //   usleep(100);
 }
 sink=sum;
}
void baseLine(int stride){
  volatile int sink;
  int sum;
  for(int i=0;i<size;i+=stride){
    sum+=rand();
  }
 sink=sum;
}
void rStride(int * arr, int stride){

  volatile int sink;
  int sum;
  for(int i=0;i<size;i+=stride){  
    sum+=arr[rand()%size];
  }
   //   usleep(100);

 sink=sum;
}
void mpre(){
  int* arr= aligned_alloc(64, size*sizeof(int));
  volatile int sink;
  int sum;
  for(int i=0;i<(size);i++){
    __builtin_prefetch(arr+i);
    sum+=i;
  }
  sink=sum;
}

void m(){
  int* arr= aligned_alloc(64, size*sizeof(int));
  volatile int sink;
  int sum;
  for(int i=0;i<(size);i++){
    /*    if(!(i%(size>>6))){
      usleep(5000);
      }*/
    sum=arr[(i*offset)%myMod]+=i;
    for(int j=i+1;j<MIN(i+mp, size);j++){
    __builtin_prefetch(arr+((size>>5)+j*offset)%myMod);
    }
    sum=arr[((size>>5)+i*offset)%myMod]+=i;
    for(int j=i+1;j<MIN(i+mp, size);j++){
    __builtin_prefetch(arr+((size>>4)+j*offset)%myMod);
    }
    sum=arr[((size>>4)+i*offset)%myMod]+=i;
    for(int j=i+1;j<MIN(i+mp, size);j++){
    __builtin_prefetch(arr+((size>>4)+(size>>5)+j*offset)%myMod);
    }
    sum=arr[((size>>4)+(size>>5)+i*offset)%myMod]+=i;
    for(int j=i+1;j<MIN(i+mp, size);j++){
    __builtin_prefetch(arr+((size>>2)+i*offset)%myMod);
    }
    sum=arr[((size>>2)+i*offset)%myMod]+=i;
    for(int j=i+1;j<MIN(i+mp, size);j++){
    __builtin_prefetch(arr+((size>>2)+(size>>5)+j*offset)%myMod);
    }
    sum=arr[((size>>2)+(size>>5)+i*offset)%myMod]+=i;
    for(int j=i+1;j<MIN(i+mp, size);j++){
    __builtin_prefetch(arr+((size>>4)+(size>>2)+j*offset)%myMod);
    }
    sum=arr[((size>>4)+(size>>2)+i*offset)%myMod]+=i;
    for(int j=i+1;j<MIN(i+mp, size);j++){
    __builtin_prefetch(arr+((size>>4)+(size>>2)+(size>>5)+j*offset)%myMod);
    }
    sum=arr[((size>>4)+(size>>2)+(size>>5)+i*offset)%myMod]+=i;
    for(int j=i+1;j<MIN(i+mp, size);j++){
    __builtin_prefetch(arr+((size>>1)+(size>>5)+j*offset)%myMod);
    }
    sum=arr[((size>>1)+(size>>5)+i*offset)%myMod]+=i;
    
  }
  sink=sum;

}

void sanityCheck(){

  //any events should be noise compared to other runs
  usleep(1000000);
}
int main(int argc, char** argv){
  cpu_set_t *set=calloc(1, sizeof(cpu_set_t));
  int cpu= 0;
  int* arr= aligned_alloc(4096, size*sizeof(int));
  int* arr2= aligned_alloc(4096, size*sizeof(int));
  int s=atoi(argv[1]);
  CPU_ZERO(set);
  CPU_SET(cpu, set);
  sched_setaffinity(getpid(),sizeof(set),set);
  for(int i=0;i<8;i++){
            rStride(arr,s);
    //    baseLine(s);
    //     mStride(arr,s);
    //    m();
  }
  
  // m();
  //  mpre();
  //  sanityCheck();
}
