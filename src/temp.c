#include <dirent.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <malloc.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include "temp.h"
#include "util.h"

#define VERSION "0.1"




void initTemp(int verbose, int trials, int numThr){
    if(!getCores()){
      printf("Couldnt find cores on your machine\n");
      return;
    }
    if(setPath(verbose)){
      return;
    }
    tempStart=malloc(numThr*sizeof(double*));
    tempEnd=malloc(numThr*sizeof(double*));
    for(int i =0;i<numThr;i++){
    tempStart[i]=malloc(trials*sizeof(double));
    tempEnd[i]=malloc(trials*sizeof(double));
    }
}
//cant use number of processors because hyperthread (on my machine at least 8 processors 4 cores)
//also seems like processors 4-7 are on core procNum-4 so only first 4 threads will need to monitor core temp.
int getCores(){
  FILE* fp= fopen("/proc/cpuinfo","r");
  char buf[32]="";
  while(fgets(buf, 32,fp)){
    if(!strncmp(buf,"cpu cores",9)){
      numCores=atoi(buf+12);
      return 1;
    }
  }
  return 0;
}


int setPath(int verbose){
  DIR* d=NULL;
  int cores=0;
  int success=0;
  struct dirent *dir;
 startDir:;
  d=opendir(tempPath);
  if(d){
    while((dir=readdir(d))!=NULL){
      if(!strncmp("hwmon",dir->d_name,5)){
	strcat(dir->d_name,"/");
	strcat(tempPath,dir->d_name);
	closedir(d);
	goto startDir;
      }
    }
  }
    char checkPath[128]="";
    int max=0;
    while(1){

      sprintf(checkPath,"%stemp%d_label",tempPath,max);
      if(verbose){
      printf("Checking: %s\n", checkPath);
      }
      if(access(checkPath,F_OK)!=-1){
	success=1;
	FILE* fp=fopen(checkPath,"r");
	    if(fp){
	      char buf[32]="";
	      if(fgets(buf,32,fp)!=NULL){
		if(!strncmp(buf,"Core",4)){
		  cores+=1<<atoi(buf+4);
		}
	      }
	    }
      }
      else{
	coreOffset++;
	if(success){
	  break;
	}
      }
      max++;
    }

    if(cores!=((1<<numCores)-1)){
      printf("Couldnt find all cores %x vs %x\n", cores,((1<<numCores)-1));
      return -1;
    }
    return 0;
}
void doTemps(int verbose, int start, int index, int trial, int numThr){
  double** curPtr=NULL;
  curPtr=tempEnd;
  if(start){
    curPtr=tempStart;
  }
  
  char path[128]="";
  int iStart=index, iEnd=index+1;
  if(index==-1){
    iStart=0;
    iEnd=numThr;
  }
  for(int i =iStart;i<iEnd;i++){
    sprintf(path,"%s/temp%d_input", tempPath, i%numCores+coreOffset);
    FILE* fp=fopen(path, "r");
    if(fp){
    fscanf(fp,"%lf", &curPtr[i][trial]);
    curPtr[i][trial]=curPtr[i][trial]/1000.0;
    }
    fclose(fp);
  }
}
void printTempsResults(int numThr, int trial){
    for(int i =0;i<numThr;i++){
      printf("START: Thread %d on core %d: Avg %lf Min-Max: %lf-%lf\n",i, i%numCores,
	     getMeanFloat(tempStart[i], trial) ,
	     getMinFloat(tempStart[i],trial),
	     getMaxFloat(tempStart[i],trial));
    }
    for(int i =0;i<numThr;i++){
      printf("END: Thread %d on core %d: Avg %lf Min-Max: %lf-%lf\n",i, i%numCores,
	     getMeanFloat(tempEnd[i],trial) ,
	     getMinFloat(tempEnd[i],trial),
	     getMaxFloat(tempEnd[i],trial));
    }
}
void printTempsV(int numThr, int trial){
  for(int i =0;i<numThr;i++){
    printf("Thread %d on core %d: %f -> %f\n", i, i%numCores, tempStart[i][trial], tempEnd[i][trial]);

      }
}
void enforceTemp(int verbose,int tid, int numThr){
      int loopNum=0;
      while(1){
	int cont=0;
	if(loopNum){
	  sleep(1);
	}
	doTemps(verbose, 0, tid, 0, numThr);
	for(int i =tid;i<tid+1;i++){
	  if(verbose){
	    printf("%d: Thread %d on core %d -> start %f vs cur %f\n", loopNum, i, i%numCores, tempStart[i][0], tempEnd[i][0]);
	  }
	  if(tempEnd[i][0]>(1.1*tempStart[i][0])){
	    cont=1;
	  }
	}
	loopNum++;
	if(!cont){
	break;
	}
      }
  }

