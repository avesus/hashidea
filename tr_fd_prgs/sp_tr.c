#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include<sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#define dist 10

int main(int argc, char** argv){
  srand(time(NULL));
  struct stat st = {0};
  char new_dir[32]="", check_dir[64]="";
  strncpy(new_dir, argv[1]+10, strlen(argv[1]+10)-4);
  sprintf(check_dir,"../spread_traces/%s", new_dir);
 tryDir:;
  if(stat(check_dir,&st)==-1){
    mkdir(check_dir, 0777);
  }
  else{
    char rms[64]="";
    sprintf(rms,"exec rm %s/*",check_dir);
    system(rms);
    remove(check_dir);
    goto tryDir;
  }
  char buf[64]="";
  int fp = open(argv[1],O_CREAT | O_RDONLY, S_IRUSR | S_IWUSR);
  int* pt = (int*)malloc(sizeof(int)*32);
  char topen[64]="";
  for(int i =0;i<32;i++){
    sprintf(topen,"%s/p%d.txt", check_dir, i);
    pt[i]=open(topen,O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
  }
  int re=0;
  while((re=read(fp, &buf, 22))>0){
    for(int i =0;i<(rand()%dist)+1;i++){
      write(pt[rand()%32], buf, 22);
      //      fprintf(pt[rand()%32],"%s", buf);
    }

  }
}
