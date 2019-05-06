#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>


#define r 10
#define h 3
int main(int argc, char** argv){
  if(argc!=2){
    return 0;
  }
  srand(time(NULL));
  int size=atoi(argv[1]+13);
  FILE* fp =fopen(argv[1],"r");
  char buf[32]="";
  sprintf(buf,"../traces/t_%s",argv[1]+10);
  FILE* tp=fopen(buf,"wa");
  unsigned long* hist=(unsigned long*)malloc(size*sizeof(unsigned long));
  int index=0;
  char line[32]="";
  char * end;
  while(fgets(line,32, fp)!=NULL){
    hist[index]=strtoull(line, &end, 10);
    fprintf(tp,"A %lu\n", hist[index]);

    for(int i=0;i<rand()%r;i++){
      if(rand()%h){
	unsigned long temp=rand();
	if(argv[1][10]=='u'){
	temp*=rand();
	}
	fprintf(tp,"Q %lu\n", temp);
      }
      else{
	fprintf(tp,"T %lu\n", hist[rand()%index]);
      }
      index++;
    }
  }

}
