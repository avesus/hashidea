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
#include <assert.h>

#include "temp.h"
#include "stat.h"
#include "util.h"


extern int verbose;

#define VERSION "0.1"

#define PATHLEN 256
// path to directory with core files
static  char tempPath[PATHLEN]="/sys/devices/platform/coretemp.0/";
// the first number used to find the file with a core
static  int coreOffset=0;	
// number of physical cores in system
static  int numCores=0;

//get num cores (called in initTemp)
static int getCores(void);

//set path for tempPath (called in initTemp)
static int setPath(void);


void
printNdouble(FILE* f, const char* prompt, int n, double* ds) {
  fputs(prompt, f);
  for (int i=0; i<n; i++) fprintf(f, "\t%lf", ds[i]);
  fputs("\n", f);
}

//initializes variables. Gets number of cores, sets variables for finding tempature files
//and mallocs arrays for storing information
int
initTemp(int trials, int numThr) {
  if (!getCores()) {
    printf("Couldnt find cores on your machine\n");
    return -1;
  }
  if (setPath()) {
    return -1;
  }
  return 0;
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
getCores(void) {
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
setPath(void) {
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
  closedir(d);

  //once through all the hwmon* get offset into temp*_label files
  //basically temp0_label does not exist on my computer and temp1_label is not related to a core
  //starts on my comp for temp2_label (might be different on yours) so I read through temp%d_label
  //until matches with Core* then continue reading until file does not exist (i.e on 4 core
  //machine would read until temp5_label. Once file does not exist but after succesful
  //reads (so could start at temp50_label) will exist while(1) loop. 
  char checkPath[PATHLEN];
  int max=0;
  while (1) {
    sprintf(checkPath,"%stemp%d_label",tempPath,max);
    if (verbose) {
      printf("Checking: %s\n", checkPath);
    }
    //check if file exists
    if (access(checkPath,F_OK)!=-1) {
      success=1;
      FILE* fp=fopen(checkPath,"r");
      if (fp) {
	char buf[32]="";
	if (fgets(buf,32,fp)!=NULL) {
	  if (!strncmp(buf,"Core",4)) {
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
      if (success) {
	break;
      }
    }
    //If didnt find as many
    //files as cores on machine will return -1. Returns 0 on success.
    max++;

  }
  if (cores != ((1<<numCores)-1)) {
    printf("Couldnt find all cores %x vs %x\n", cores,((1<<numCores)-1));
    return -1;
  }
  return 0;
}

double
getTempFromCore(int coreid) {
  char path[PATHLEN];
  double temp = 0;
  
  sprintf(path,"%s/temp%d_input", tempPath, coreid);
  FILE* fp=fopen(path, "r");
  if (fp) {
    int x = fscanf(fp,"%lf", &temp);
    if (x != 1) {
      die("Failed to read temperature from [%s]\n", path);
    }
    fclose(fp);
  } else {
      die("Failed to open [%s]\n", path);
  }
  return temp/1000.0;
}

//reads temps for cores associated with  `numThr` threads starting at thread `index`.
//store result in dest[index]
void 
doTemps(int index, double* dest, int numThr) {
  //fprintf(stderr, "doTemp(%d,%p,%d)\n", index, dest, numThr);
  int iStart=index;
  int iEnd=index+numThr;
  long long int gotit = 0;
  assert(numThr <= (8*sizeof(unsigned long long))); /* if running on more threads need better way to avoid duplicate queries */
  
  for (int i = iStart; i < iEnd; i++) {
    int coreIndex = i%numCores;
    double temp = getTempFromCore(coreIndex+coreOffset);
    dest[i] = temp;
  }
}

#define tempOffset(fld, thr) (((int)(((char*)&(trialData->fld)) - (char*)(&(trialData->time))))+(thr*sizeof(double)))

extern int nthreads;

void 
printPTI(FILE* f, PerTrialInfo* p) {
  fprintf(f, "%p: t:%llu, m:%lf, be:%p bs:%lf,%lf st:%p et:%p\n\tBE:",
	  p, p->time, p->memutils, p->barrierEnds, p->barrierGaps.maxgap, p->barrierGaps.medgap, p->startTemp, p->endTemp);
  for (int i=0; i<nthreads; i++) fprintf(f, "\t%lld", p->barrierEnds[i]);
  fprintf(f, "\n\tST:");
  for (int i=0; i<nthreads; i++) fprintf(f, "\t%lf", p->startTemp[i]);
  fprintf(f, "\n\tET:");
  for (int i=0; i<nthreads; i++) fprintf(f, "\t%lf", p->endTemp[i]);
  fprintf(f, "\n");
}

static void
gatherTemps(PerTrialInfo* ptd, int offset, int tid, int n, double* data)
{
  //printf("%d:\t", tid);
  for (int i=0; i<n; i++) {
    //printPTI(stderr, ptd+i);
    data[i] = (*((double**)(((char*)(ptd+i))+offset)))[tid];
    //printf("\t%lf", data[i]);
  }
  //printf("\n");
}


//prints temps and some relevant staticical data
void printTempsResults(const char* desc, int numThr, int trial, int mode) {
  // collect data and print per thread info
  double* data = mycalloc(trial*numThr*2, sizeof(double));
  for (int i =0; i<numThr; i++) {
    double* thisdata = data+(i*trial);
    gatherTemps(trialData, statOffset(startTemp), i, trial, thisdata);
    if (!mode)
      printf("START: Thread %d on core %d: Avg %lf Min-Max: %lf-%lf\n", 
	     i, i%numCores,
	     getMeanD(thisdata, trial) ,
	     getMinD(thisdata, trial),
	     getMaxD(thisdata, trial));
  }
  for (int i =0; i<numThr; i++) {
    double* thisdata = data+((numThr+i)*trial);
    gatherTemps(trialData, statOffset(endTemp), i, trial, thisdata);
    if (!mode)
      printf("END: Thread %d on core %d: Avg %lf Min-Max: %lf-%lf\n", 
	     i, i%numCores,
	     getMeanD(thisdata, trial) ,
	     getMinD(thisdata,  trial),
	     getMaxD(thisdata,  trial));
  }
  // now get summary info
  double meanStart = getMeanD(data, trial*numThr);
  double meanEnd = getMeanD(data+(trial*numThr), trial*numThr);
  // now get deltas
  for (int i=0; i<trial*numThr; i++)
    data[i] = data[i+(trial*numThr)] - data[i];
  double meanDelta = getMeanD(data, trial*numThr);
  double minDelta = getMinD(data, trial*numThr);
  double maxDelta = getMaxD(data, trial*numThr);  
  printf("%s,\ttemp, start:%lf, end:%lf, minDelta:%lf, maxDelta:%lf, meanDelta:%lf, meanCoolWait:%lf\n",
	 desc, meanStart, meanEnd, minDelta, maxDelta, meanDelta, getMeanFloat(trialData, statOffset(avgCoolWait), trial));
  free(data);
}

//prints temps without relevant statical data (basically for verbose mode)

void 
printTempsV(PerTrialInfo* pti, int numThr)
{
  fprintf(stderr, "PTV: tdp:%p, tdp->BE:%p, tdp:ST:%p, tdp:ET:%p\n", pti, pti->barrierEnds, pti->startTemp, pti->endTemp);
  for (int i =0;i<numThr;i++) {
    printf("Thread %d on core %d: %f -> %f\n", i, i%numCores, pti->startTemp[i], pti->endTemp[i]);
  }
}

static double* enforcedTemps;
static double deltaEnforcedTemp;
static double* nowTemps;

static void
getTemps(double* buffer, int nthreads) {
  unsigned long long gotit = 0;
  assert(nthreads <= (8*sizeof(unsigned long long))); /* if running on more threads need better way to avoid duplicate queries */

  for (int i=0; i<nthreads; i++) {
    int idx = i%numCores;
    if (gotit & (1 << idx)) continue;
    gotit |= (1 << idx);
    buffer[i] = getTempFromCore(idx+coreOffset);
  }
}

void
setEnforcedTemps(double delta, int nthreads) 
{
  enforcedTemps = calloc(nthreads, sizeof(double));
  nowTemps = calloc(nthreads, sizeof(double));
  deltaEnforcedTemp = delta;
  getTemps(enforcedTemps, nthreads);
  printNdouble(stderr, "Enforcing temp:", nthreads, enforcedTemps);
}

#define MaxSleepBeforeExit 1000

static int showenforcedtemps = 1;

// stays in while (1) with a sleep(1) inside until the current
// temperature is less than 1.1*StartingTemp for a given core.  Wait a
// maximum of `maxWait` seconds before terminating program
void 
enforceTemps(int numThr, int maxWait) 
{
  int loopNum=maxWait;
  while (1) {
    int cont=0;
    if (loopNum > 0) {
      //sleep here to let cool off between loops, can adjust this to fit your needs
      sleep(1);
    }
    // get current temperatures
    getTemps(nowTemps, numThr);
    for (int t=0; t<numThr; t++) {
      int i = t%numCores;
      if (verbose) {
	printf("%d: core %d -> start %f vs cur %f\n", loopNum, i, enforcedTemps[i], nowTemps[i]);
      }
      //compare, if cont is set will redo loop, else will break
      if (nowTemps[i] > (deltaEnforcedTemp * enforcedTemps[i])) {
	cont=1;
      }
    }
    loopNum--;
    if (!cont) {
      // all cores are cool enough
      tdp->avgCoolWait = (double)(maxWait - loopNum);
      char buffer[64];
      sprintf(buffer, "After enforcing temp(%7.3lf):", tdp->avgCoolWait);
      printNdouble(stderr, buffer, numThr, nowTemps);
      break;
    }
    if (loopNum <= 0) {
      fprintf(stderr, 
	      "Exceeded %d loops without reaching cool down temperature\n", maxWait);
      for (int i=0; i<numCores; i++) {
	fprintf(stderr, "\tcore %d: %lf isn't lower than %lf*%lf\n", i, nowTemps[i], deltaEnforcedTemp, enforcedTemps[i]);
      }
      exit(-1);
    }
  }
}
