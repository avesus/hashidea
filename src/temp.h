#ifndef _TEMP_H_
#define _TEMP_H_

// package to monitor temperature of cores in a multicore processor.  
// Specific to linux.  Tested on Ubuntu.

// call before anything else
void initTemp(int trials, int numThr);

void printTempsResults(int numThr, int trial);
void printTempsV(int numThr, int trial);

// NOAH PLEASE ADD COMMENT HERE
// can you explain what 4 arguments mean?
void doTemps(int start, int index, int trial, int numThr);

// NOAH MAKE SURE THIS IS RIGHT
// make sure temperature is XXX within initial temperature (that in tempStart[x][0]?)
void enforceTemp(int tid,int numThr);

#endif
