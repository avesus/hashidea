#ifndef _TEMP_H_
#define _TEMP_H_

// package to monitor temperature of cores in a multicore processor.  
// Specific to linux.  Tested on Ubuntu.


//prints results after all trials
void printTempsResults(int numThr, int trial);

//prints temp dif for given trial
void printTempsV(int numThr, int trial);

//initializes num cores, path to core temps and memory for data storage
void initTemp(int verbose, int trials, int numThr);

//get num cores (called in initTemp)
static int getCores();

//set path for tempPath (called in initTemp)
static int setPath(int verbose);

//gets the data from the files, read .c for explination of args
void doTemps(int verbose, int start, int index, int trial, int numThr, int tracking);

//wont let thread past until its temp is under threshold (1 sec sleep between tests).x
void enforceTemp(int verbose,int tid,int numThr);

#endif
