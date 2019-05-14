#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>





void swap(unsigned long *a, unsigned long*b){
  unsigned long c=*a;
  *a=*b;
  *b=c;
  

}

unsigned long work(unsigned long * arr, unsigned long min, unsigned long max){
  unsigned long pivot=arr[min+(max-min)/2];
  min--;
  max++;
  while(1){
    min++;
    while(arr[min]<pivot){
      min++;
    }
    max--;
    while(arr[max]>pivot){
      max--;
    }
    if(min>=max){
      return max;
    }
    swap(arr+min, arr+max);
  }


}
void recursive(unsigned long * arr, unsigned long min, unsigned long max){
  if(max>min){
    unsigned long mid =work(arr, min, max);

    recursive(arr, min, mid);
    recursive(arr, mid+1, max);


  }

}




int main(){

  FILE* fp=fopen("output.txt","r");
  char buf[32]="";

  int j=0;
  // fgets(buf, 32, fp);
  int count=100;//atoi(buf);
  unsigned long* arr=(unsigned long*)malloc(sizeof(unsigned long*)*count);
  while(fgets(buf, 32, fp)!=NULL){
    char* end;
    unsigned long temp=strtoull(buf, &end, 10);
    arr[j]=temp;
    j++;
  }


  // printf("arr[8372151]=%lu\n", arr[8372150]);
   recursive(arr, 0, j-1);
   FILE* res= fopen("new_res.txt", "a");
   int i=1;
   for(i =1;i<j;i++){
     if(arr[i]==arr[i-1]){
       fprintf(res,"messed up on count = %d on numbers arr[%d]=%lu and arr[%d]=%lu\n",count, i, arr[i], i-1, arr[i-1]);
     }

     if(arr[i]<arr[i-1]){
       fprintf(res,"Quicksort failed!\n");
     }
   }
   fprintf(res,"-------------------- END CHECK -----------------\n");
}
