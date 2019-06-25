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
extern int verbose;

#define VERSION "0.1"

// path to directory with core files
static  char tempPath[128]="/sys/devices/platform/coretemp.0/";
// the first number used to find the file with a core
static  int coreOffset=0;	
// number of physical cores in system
static  int numCores=0;
// harness specific data collection pointers.  Each points to numthreads array's of ntrials doubles.
static  double ** tempStart=NULL;
static  double ** tempEnd=NULL;



//initializes variables. Gets number of cores, sets variables for finding tempature files
//and mallocs arrays for storing information
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
      tempStart[i]=malloc((trials+1)*sizeof(double));
      tempEnd[i]=malloc((trials+1)*sizeof(double));
    }
}

// set number of physical cores (not hyperthreading count).  
// Returns 1 on success (and sets `numCores`), 0 on failure
//
// Depends on `/proc/cpuinfo` to be available
// cant use number of processors because hyperthread (on my machine at
// least 8 processors 4 cores) also seems like processors 4-7 are on
// core procNum-4 so only first 4 threads will need to monitor core
// temp.
static int 
getCores(){
  FILE* fp= fopen("/proc/cpuinfo","r");
  char buf[32]="";
  while (fgets(buf, 32,fp)) {
    if (!strncmp(buf,"cpu cores",9)) {
      numCores=atoi(buf+12);
      return 1;
    }
  }
  return 0;
}



//Finds the files that contain temp information for each core
//(does this by finding the path starting at
// /sys/devices/platform/coretemp.0/ then trying to find hwmon* until its reached
//reach dir that containts temp*_type files. Then finds the temp*_type files that
//correspond to cores and ensures it can find all cores for later. Will print error and
//return -1 if it cant find the temperature file for each core (0 returned on success)
//there is a goto in there so you might want to delete the entire file?
static int 
setPath(int verbose) {
  DIR* d=NULL;
  int cores=0;
  int success=0;
  struct dirent *dir;
 startDir:;
  //open dir and read its contents
  d=opendir(tempPath);
  if (d) {
    while ((dir=readdir(d)) != NULL) {
      // check to see if hwmon is in this directory
      if (!strncmp("hwmon",dir->d_name,5)) {
	strcat(tempPath,dir->d_name);
	strcat(tempPath,"/");
	closedir(d);
	//SORRY
	goto startDir;
      }
    }
  }

  //once through all the hwmon* get offset into temp*_label files
  //basically temp0_label does not exist on my computer and temp1_label is not related to a core
  //starts on my comp for temp2_label (might be different on yours) so I read through temp%d_label
  //until matches with Core* then continue reading until file does not exist (i.e on 4 core
  //machine would read until temp5_label. Once file does not exist but after succesful
  //reads (so could start at temp50_label) will exist while(1) loop. 
    char checkPath[128]="";
    int max=0;
    while(1){

      sprintf(checkPath,"%stemp%d_label",tempPath,max);
      if(verbose){
      printf("Checking: %s\n", checkPath);
      }
      //check if file exists
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
	//core offset incremented so later when trying to do core 3 can do
	//threadNum%numCores+coreOffset as %d in temp%d_input
	coreOffset++;

	//success is set to 1 after the temp%d_label file corresponding to a core was found
	//next time a file does not exist will break from while(1)
	if(success){
	  break;
	}
      }
      //If didnt find as many
      //files as cores on machine will return -1. Returns 0 on success.
      max++;

    }
    if(cores!=((1<<numCores)-1)){
      printf("Couldnt find all cores %x vs %x\n", cores,((1<<numCores)-1));
      return -1;
    }
    return 0;
}

//reads temps for core number index (or for all cores index is set to -1).
//trial is position in the 2d array to store the info, numThr is the thread this is foor
//and start tells whether to store in the start temp array or end temp array.
void 
doTemps(int verbose, int start, int index, int trial, int numThr, int tracking) {
  double** curPtr=NULL;
  curPtr=tempEnd;
  if(start){
    curPtr=tempStart;
  }
  
  char path[128]="";
  int iStart=index, iEnd=index+1;
  if(index==-1) {
    iStart=0;
    iEnd=numThr;
  }
  for(int i =iStart;i<iEnd;i++){
    sprintf(path,"%s/temp%d_input", tempPath, i%numCores+coreOffset);
    FILE* fp=fopen(path, "r");
    if(fp){
      //tracking basically ensures that position 0 in array will always be free for enforcing temp
      //constaints
    fscanf(fp,"%lf", &curPtr[i][trial+tracking]);

    //stored as celcius*1000 (but never does decimals so why x1000??)
    curPtr[i][trial+tracking]=curPtr[i][trial+tracking]/1000.0;
    }
    fclose(fp);
  }
}

//prints temps and some relevant staticical data
void printTempsResults(int numThr, int trial){
    for(int i =0;i<numThr;i++){
      printf("START: Thread %d on core %d: Avg %lf Min-Max: %lf-%lf\n",i, i%numCores,
	     getMeanFloat(tempStart[i]+1, trial) ,
	     getMinFloat(tempStart[i]+1,trial),
	     getMaxFloat(tempStart[i]+1,trial));
    }
    for(int i =0;i<numThr;i++){
      printf("END: Thread %d on core %d: Avg %lf Min-Max: %lf-%lf\n",i, i%numCores,
	     getMeanFloat(tempEnd[i]+1,trial) ,
	     getMinFloat(tempEnd[i]+1,trial),
	     getMaxFloat(tempEnd[i]+1,trial));
    }
}

//prints temps without relevant statical data (basically for verbose mode)
void 
printTempsV(int numThr, int trial) {
  for(int i =0;i<numThr;i++){
    printf("Thread %d on core %d: %f -> %f\n", i, i%numCores, tempStart[i][trial+1], tempEnd[i][trial+1]);

  }
}


//stays in while (1) with a sleep(1) inside until the current
//temperature is less than 1.1*StartingTemp for a given core.
void enforceTemp(int verbose,int tid, int numThr){
      int loopNum=0;
      while(1){
	int cont=0;
	if(loopNum){
	  //sleep here to let cool off between loops, can adjust this to fit your needs
	  sleep(1);
	}

	//get time
	doTemps(verbose, 0, tid, 0, numThr, 0);
	for(int i =tid;i<tid+1;i++){
	  if(verbose){
	    printf("%d: Thread %d on core %d -> start %f vs cur %f\n", loopNum, i, i%numCores, tempStart[i][0], tempEnd[i][0]);
	  }
	  //compare, if cont is set will redo loop, else will break
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
