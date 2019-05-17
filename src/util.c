#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <malloc.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include "timing.h"
#include "util.h"

const char* progname;

void
dieOnErrno(const char* fn, int ln, int en, const char* msg, ...)
{
    va_list ap;

    va_start(ap, msg);
    fprintf(stderr, "%s:%d:", __FILE__, __LINE__);
    vfprintf(stderr, msg, ap);
    va_end (ap);
    fprintf(stderr, " %d:%s\n", en, strerror(en));
    exit(-1);
}

  
void
die(const char* fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    fprintf(stderr, "%s: ", progname);
    vfprintf(stderr, fmt, ap);
    va_end (ap);
    fprintf(stderr, "\n");
    exit(-1);
}

////////////////////////////////////////////////////////////////
// basic helper functions

void*
myCalloc(size_t nmemb, size_t size, const char* fname, const int ln)
{
  void* p = calloc(nmemb, size);
  if (p == NULL) die("Failed to allocate memory at %s:%d", fname, ln);
  return p;
}

void 
semWait(sem_t *sem) {
  while (sem_wait(sem) == -1) {
    if (errno == EINTR) continue;
    // some other error
    errdie("Semaphore wait failure");
  }
}

void 
semPost(sem_t* sem) {
  if (sem_post(sem) == 0) return;
  errdie("semaphore post failure");
}

void
initBarrierN(Barrier* b, int n) {
  int s = pthread_barrier_init(b, NULL, n);
  if (s) errdie("Can't initialize barrier");
}

void
myBarrier(Barrier* b) {
  pthread_barrier_wait(b);
}

////////////////////////////////////////////////////////////////
// stat functions

static int nanocomp(const void* a, const void *b) {
  return *(nanoseconds*)b - *(nanoseconds*)a;
}

double
getMedian(nanoseconds* trialTimes, int trialNumber)
{
  nanoseconds* tt = mycalloc(trialNumber, sizeof(nanoseconds));
  memcpy(tt, trialTimes, trialNumber*sizeof(nanoseconds));
  qsort(tt, trialNumber, sizeof(nanoseconds), nanocomp);
  nanoseconds median;
  if (trialNumber & 1) median = tt[trialNumber >> 1];
  else median = (tt[trialNumber >> 1] + tt[(trialNumber >> 1)+1])>>1;
  free(tt);
  return median;
}

double
getMean(nanoseconds* trialTimes, int trialNumber)
{
  double total = 0.0;
  for (int x = 0; x<trialNumber; x++) {
    total += (double)trialTimes[x];
  }
  return total/(double)trialNumber;
}

double
getSD(nanoseconds* trialTimes, int trialNumber)
{
  double sum = 0.0;
  double mean;
  double sd = 0.0;
  int i;

  for(i=0; i<trialNumber; i++) {
    sum += trialTimes[i];
  }

  mean = sum/(double)trialNumber;

  for(i=0; i<trialNumber; i++)
    sd += pow(trialTimes[i] - mean, 2);

  return sqrt(sd/trialNumber);
}

double 
getMin(nanoseconds* trialTimes, int trialNumber) {
  double m = trialTimes[0];
  for(int i=0; i<trialNumber; i++)
    if (m > trialTimes[i]) m = trialTimes[i];
  return m;
}


double 
getMax(nanoseconds* trialTimes, int trialNumber) {
  double m = trialTimes[0];
  for(int i=0; i<trialNumber; i++)
    if (m < trialTimes[i]) m = trialTimes[i];
  return m;
}



double
getMedianFloat(double* trialTimes, int trialNumber)
{
  double* tt = mycalloc(trialNumber, sizeof(double));
  memcpy(tt, trialTimes, trialNumber*sizeof(double));
  qsort(tt, trialNumber, sizeof(double), nanocomp);
  double median;
  if (trialNumber & 1) median = tt[trialNumber >> 1];
  else median = (tt[trialNumber >> 1] + tt[(trialNumber >> 1)+1])/2.0;
  free(tt);
  return median;
}

double
getMeanFloat(double* trialTimes, int trialNumber)
{
  double total = 0.0;
  for (int x = 0; x<trialNumber; x++) {
    total += (double)trialTimes[x];
  }
  return total/(double)trialNumber;
}

double
getSDFloat(double* trialTimes, int trialNumber)
{
  double sum = 0.0;
  double mean;
  double sd = 0.0;
  int i;

  for(i=0; i<trialNumber; i++) {
    sum += trialTimes[i];
  }

  mean = sum/(double)trialNumber;

  for(i=0; i<trialNumber; i++)
    sd += pow(trialTimes[i] - mean, 2);

  return sqrt(sd/trialNumber);
}

double 
getMinFloat(double* trialTimes, int trialNumber) {
  double m = trialTimes[0];
  for(int i=0; i<trialNumber; i++)
    if (m > trialTimes[i]) m = trialTimes[i];
  return m;
}


double 
getMaxFloat(double* trialTimes, int trialNumber) {
  double m = trialTimes[0];
  for(int i=0; i<trialNumber; i++)
    if (m < trialTimes[i]) m = trialTimes[i];
  return m;
}

