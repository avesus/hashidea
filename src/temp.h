#ifndef _TEMP_H_
#define _TEMP_H_



static  char tempPath[128]="/sys/devices/platform/coretemp.0/";
static  int coreOffset=0;
static  int numCores=0;
static  double ** tempStart=NULL;
static  double ** tempEnd=NULL;


void printTempsResults(int numThr, int trial);
void printTempsV(int numThr, int trial);
void initTemp();
int getCores();
int setPath(int verbose);
void doTemps(int verbose, int start, int index, int trial, int numThr);
void enforceTemp(int verbose,int tid,int numThr);

#endif
