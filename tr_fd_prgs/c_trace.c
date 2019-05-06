#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include<sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#define r 10
#define h 3
int main(int argc, char** argv){
  if(argc!=2){
    return 0;
  }
  srand(time(NULL));
  int size=atoi(argv[1]+13);
  int fp =open(argv[1],O_CREAT | O_RDONLY, S_IRUSR | S_IWUSR);
  char buf[32]="";
  sprintf(buf,"../traces/t_%s",argv[1]+10);
  int tp=open(buf,O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
  char** hist=(char**)malloc(size*sizeof(char*));
  for(int i =0;i<size;i++){
    hist[i]=(char*)calloc(1,32);
  }
  int index=0;
  char line[32]="";
  char * end;
  int re=0;
  while((re=read(fp, &line, 20))>0){
    strcpy(hist[index],line);
    char tosend[32]="";
    sprintf(tosend,"A %s", hist[index]);
    write(tp, tosend,22);

    for(int i=0;i<rand()%r;i++){
      if(rand()%h){
	unsigned long temp=rand();
	if(argv[1][0]=='u'){
	temp*=rand();
	}
	sprintf(tosend,"Q %s", hist[index]);
	write(tp, tosend,22);
      }
      else{
	sprintf(tosend,"T %s", hist[index]);
	write(tp, tosend,22);
      }

    }
    index++;
  }

}
