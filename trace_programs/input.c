#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>


int main(int argc, char** argv){
  if(argc!=3){
    return 0;
  }
  int b= (argv[2][0]=='l');
  unsigned int amt= atoi(argv[1]);
  amt=1<<amt;
  char buf[32]="";
  if(b){
    sprintf(buf,"../inputs/ull%d.txt" ,amt);
  }
  else{
    sprintf(buf,"../inputs/int%d.txt" ,amt);
  }
  FILE* fp = fopen(buf,"wa");
  for(int i =0;i<amt;i++){
  unsigned long entry=rand();
  for(int j =0;j<b;j++){
    entry*=rand();
  }
  fprintf(fp, "%lu\n", entry);


  }



}
