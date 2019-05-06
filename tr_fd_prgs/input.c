#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include<sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


int main(int argc, char** argv){
  if(argc!=3){
    return 0;
  }
  srand(time(NULL));
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
  char line[33]="";
  //FILE* fp = fopen(buf,"wa");
  int fd= open(buf,  O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
  unsigned long max=0;
  max=~max;
  char maxs[32]="";
  sprintf(maxs,"%lu", max);
  int len=strlen(maxs);
  for(int i =0;i<amt;i++){
  unsigned long entry=rand();
  for(int j =0;j<b;j++){
    entry*=rand();
  }
  sprintf(line,"%lu", entry);
  int len1=strlen(line);
  for(int q =0;q<len-len1;q++){
    strcat(line, " ");
  }
  if(write(fd, line, strlen(line))==-1){
    perror("write broken\n");
    
  }
  //fprintf(fp, "%lu\n", entry);


  }



}
