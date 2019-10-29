#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#define size (1<<27)
#define lineSize 64
#define offset 16
#define myMod (size)
#define mp 50
#define MIN(X,Y)   ((X) < (Y) ? (X) : (Y))

void mStride(int stride){
  int* arr= aligned_alloc(64, size*sizeof(int));
  volatile int sink;
  int sum;
 for(int i=0;i<(size);i+=stride){

   sum=arr[i]+=i;
   //   usleep(100);
 }
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
  usleep(25000000);
}
int main(int argc, char** argv){
        mStride(atoi(argv[1]));
  // m();
  //  mpre();
  //    sanityCheck();
}
