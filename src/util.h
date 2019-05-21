#ifndef _UTIL_H_
#define _UTIL_H_

#include <pthread.h>
#include <semaphore.h>
#include <errno.h>

// Basic error reporting

#define errdie(msg, args...) dieOnErrno(__FILE__, __LINE__, errno, msg, ##args)
void dieOnErrno(const char* fn, int ln, int en, const char* msg, ...);
void die(const char* fmt, ...);
extern const char* progname;

// basic memory

#define mycalloc(x, y) myCalloc((x), (y), __FILE__, __LINE__)
void* myCalloc(size_t nmemb, size_t size, const char* fname, const int ln);

// basic pthread related functions

void semWait(sem_t *sem);
void semPost(sem_t* sem);

typedef pthread_barrier_t Barrier;
void myBarrier(Barrier* b);
void initBarrierN(Barrier* b, int n);
#define initBarrier(b) initBarrierN(b, nthreads);

// stats

void trimData(int n, long long unsigned* trialTimes, long long unsigned* trimmedTimes);

double getMedian(long long unsigned* trialTimes, int trialNumber);
double getMean(long long unsigned* trialTimes, int trialNumber);
double getSD(long long unsigned* trialTimes, int trialNumber);
double getMin(long long unsigned* trialTimes, int trialNumber);
double getMax(long long unsigned* trialTimes, int trialNumber);

double getMedianFloat(double* trialUtils, int trialNumber);
double getMeanFloat(double* trialUtils, int trialNumber);
double getSDFloat(double* trialUtils, int trialNumber);
double getMinFloat(double* trialUtils, int trialNumber);
double getMaxFloat(double* trialUtils, int trialNumber);


#endif
