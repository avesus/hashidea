#include <stdio.h>
#include <stdlib.h>

int main(){
  unsigned int max=1<<31;
  max=~max;
  printf("Iter: %u\n", max);
  int sum=0;
  int* arr=malloc(100*sizeof(int));
  for(int i =0;i<100;i++){
    arr[i]=i;
  }
  for(unsigned int i=0;i<max;i++){
    sum+=arr[i%100];
  }
  printf("Sum: %d\n", sum);
  
}
