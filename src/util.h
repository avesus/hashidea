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

double getMedian(nanoseconds* trialTimes, int trialNumber);
double getMean(nanoseconds* trialTimes, int trialNumber);
double getSD(nanoseconds* trialTimes, int trialNumber);
double getMin(nanoseconds* trialTimes, int trialNumber);
double getMax(nanoseconds* trialTimes, int trialNumber);

#endif
