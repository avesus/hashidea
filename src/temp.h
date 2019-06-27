#ifndef _TEMP_H_
#define _TEMP_H_

#include "stat.h"

// package to monitor temperature of cores in a multicore processor.  
// Specific to linux.  Tested on Ubuntu.

//prints results after all trials
void printTempsResults(const char* desc, int numThr, int trial, int mode);

//prints temp dif for given trial
void printTempsV(PerTrialInfo* pti, int numThr);

//initializes num cores, path to core temps and memory for data storage
void initTemp(int trials, int numThr);

//gets the data from the files, read .c for explination of args
void doTemps(int index, double* dest, int cnt);

//wont let thread past until its temp is under threshold (1 sec sleep between tests).x
void enforceTemps(int numThr);

// get the current temps as the enforcement limits within +-delta celcius degrees.
void setEnforcedTemps(double delta, int nthreads);

#endif
