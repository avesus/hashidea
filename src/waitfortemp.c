#include <sched.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <semaphore.h>
#include "arg.h"
#include "timing.h"
#include "stat.h"
#include "util.h"
#include <sys/sysinfo.h>
#include "dirent.h"
#include "../lib/hashtable.h"
#include "../lib/hashstat.h"
#include "dist.h"
#include "temp.h"

#define Version "0.1"
int verbose = 0;
int quiet = 0;
int timeout = 0;
int nthreads = 8;
double endtemp;
PerTrialInfo* trialData;
PerTrialInfo* tdp;

static ArgOption args[] = {
  // Kind, 	  	Method,		name,	    reqd,  variable,		help
  { KindOption,   	Set, 		"-v", 		0, &verbose, 		"Turn on verbosity" },
  { KindOption,   	Set, 		"-q", 		0, &quiet, 		"Be as silent as possible" },
  { KindOption,   	Integer,	"-t", 		0, &timeout, 		"seconds to wait" },
  { KindOption,   	Integer,	"-n", 		0, &nthreads, 		"number of threads" },
  { KindHelp,     	Help, 		"-h" },
  { KindPositional,	Double,		"xx", 		1, &endtemp, 		"temp to wait for" },
  { KindEnd }
};
static ArgDefs argp = { args, "Wait until cores are all <= a given temp, or timeout", Version, NULL };

int
main(int argc, char** argv)
{
  progname = argv[0];
  ArgParser* ap = createArgumentParser(&argp);
  int ok = parseArguments(ap, argc, argv);
  if (ok) die("Error parsing arguments");
  if (verbose) printf("Will wait upto %d seconds for all cores to be at or below %lf C\n", timeout, endtemp);
  if (initTemp(0, nthreads) != 0)
    die("Can't init temp code");
  double* temps = (double*)calloc(nthreads, sizeof(double));
  int allok = 1;
  for (int i=0; (timeout == 0)||(i < timeout); i++) {
    doTemps(0, temps, nthreads);
    if (verbose || (!quiet && ((i>1)&&((i%10)==0)))) {
      printf("Target:%lf", endtemp);
      for (int t=0; t<nthreads; t++) printf("\t%5.1f", temps[t]);
      printf("\n");
    }
    allok = 1;
    for (int t=0; t<nthreads; t++) if (temps[t] > endtemp) allok=0;
    if (allok) break;
    sleep(1);
  }
  if (!quiet && (allok == 0)) {
    printf("Failed to reach target temperature:");
    for (int t=0; t<nthreads; t++) printf("\t%5.1f", temps[t]);
    printf("\n");
  }
  if (allok) return 0;
  return 1;
}


// so we can link

void
startTimer(TimingStruct* ts) {
}

long long unsigned 
endTimer(TimingStruct* ts) {
  return 0;
}
