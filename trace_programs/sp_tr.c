#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#define dist 10

int main(int argc, char** argv){
  srand(time(NULL));
  struct stat st = {0};
  char new_dir[32]="", check_dir[64]="";
  strncpy(new_dir, argv[1]+7, strlen(argv[1]+7)-4);
  sprintf(check_dir,"spread_traces/%s", new_dir);
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
  FILE* fp = fopen(argv[1],"r");
  FILE** pt = (FILE**)malloc(sizeof(FILE*)*32);
  char topen[64]="";
  for(int i =0;i<32;i++){
    sprintf(topen,"%s/p%d.txt", check_dir, i);
    pt[i]=fopen(topen,"wa");
  }
  while(fgets(buf, 64, fp)!=NULL){
    for(int i =0;i<(rand()%dist)+1;i++){
      fprintf(pt[rand()%32],"%s", buf);
    }

  }
}
