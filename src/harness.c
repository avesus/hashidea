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
#include "util.h"


#define Version "0.1"

////////////////////////////////////////////////////////////////
// arguments, description, version, etc.

int verbose = 0;
int level = 0;
int quiet = 0;
int timeit = 0;
int randomSeed = 0;
int nthreads = 1;
int showthreadattr = 0;
int trialsToRun = 0;
double stopError = 0.0;
int numInsertions = 100;

int trialNumber = 0;
nanoseconds* trialTimes;

const char*
setRandom(int argc, char** argv)
{
  static char buffer[32];
  if (argc == ArgGetDefault) {
    sprintf(buffer, "%d", randomSeed);
    return buffer;
  } else if (argc == ArgGetDesc) {
    return "<int>";
  }
  randomSeed = atoi(argv[1]);
  return (const char*)1;
}

static ArgOption args[] = {
  // Kind, 	Method,		name,	    reqd,  variable,		help
  { KindOption, Set, 		"-v", 		0, &verbose, 		"Turn on verbosity" },
  { KindOption, Set, 		"-vt", 		0, &showthreadattr, 	"Turn on verbosity" },
  { KindOption, Integer, 	"-l", 		0, &level, 		"Level" },
  { KindOption, Integer, 	"--inserts",	0, &numInsertions,	"total number of insertions" },
  { KindOption, Integer, 	"--trials", 	0, &trialsToRun,	"Number of trials to run (0 means 1 or use --terror" },
  { KindOption, Double, 	"--terror", 	0, &stopError, 		"Run trials til get to error below this (0 means use --trials)" },
  { KindOption, Set, 		"-q", 		0, &quiet, 		"Run Silently - no output (useful for timing)" },    
  { KindOption, Set, 		"-T", 		0, &timeit, 		"Time the run" },    
  { KindOption, Function, 	"-r", 		0, &setRandom, 		"Set random seed (otherwise uses time)" },    
  { KindOption, Integer, 	"-t", 		0, &nthreads, 		"Number of threads" },
  { KindHelp,   Help, 		"-h" },  
  { KindEnd }
};
static ArgDefs argp = { args, "Harness for parallel hashing", Version };

pthread_t* threadids;		/* array of thread ids */
sem_t threadsDone;		/* used to signal when all threads are done */
int notDone = 1;		/* set to 0 when we are all done */
__thread int threadId;		/* internal thread id */
pthread_mutex_t showinfo_mutex = PTHREAD_MUTEX_INITIALIZER; /* used for showing thread info */
Barrier endLoopBarrier;
Barrier loopBarrier;

void
showThreadInfo(void)
{
  int s, i;
  size_t v;
  void *stkaddr;
  struct sched_param sp;
  pthread_attr_t attr;
  int result;
  const char* prefix = "\t";
  
  pthread_mutex_lock(&showinfo_mutex);  
  s = pthread_getattr_np(pthread_self(), &attr);
  if (s) errdie("getattr");
  
  s = pthread_attr_getdetachstate(&attr, &i);
  if (s) errdie("pthread_attr_getdetachstate");
  printf("Thread %d\n", threadId);
  printf("%sId                  = %lx\n", prefix, pthread_self());

  {
    cpu_set_t cpuset;
    char* before = " ";
    s = pthread_attr_getaffinity_np(&attr, sizeof(cpu_set_t), &cpuset);
    if (s) errdie("pthread_attr_getaffinity_np");
    printf("%sCPUs (%3d)          =", prefix, CPU_COUNT(&cpuset));
    for (int i=0; i<CPU_SETSIZE; i++) {
      if (CPU_ISSET(i, &cpuset)) {
	printf("%s%d", before, i);
	before = ", ";
      }
    }
    printf("\n");
  }

  printf("%sDetach state        = %s\n", prefix,
	 (i == PTHREAD_CREATE_DETACHED) ? "PTHREAD_CREATE_DETACHED" :
	 (i == PTHREAD_CREATE_JOINABLE) ? "PTHREAD_CREATE_JOINABLE" :
	 "???");

  s = pthread_attr_getscope(&attr, &i);
  if (s)
    errdie("pthread_attr_getscope");
  printf("%sScope               = %s\n", prefix,
	 (i == PTHREAD_SCOPE_SYSTEM)  ? "PTHREAD_SCOPE_SYSTEM" :
	 (i == PTHREAD_SCOPE_PROCESS) ? "PTHREAD_SCOPE_PROCESS" :
	 "???");

  s = pthread_attr_getinheritsched(&attr, &i);
  if (s)
    errdie("pthread_attr_getinheritsched");
  printf("%sInherit scheduler   = %s\n", prefix,
	 (i == PTHREAD_INHERIT_SCHED)  ? "PTHREAD_INHERIT_SCHED" :
	 (i == PTHREAD_EXPLICIT_SCHED) ? "PTHREAD_EXPLICIT_SCHED" :
	 "???");

  s = pthread_attr_getschedpolicy(&attr, &i);
  if (s)
    errdie("pthread_attr_getschedpolicy");
  printf("%sScheduling policy   = %s\n", prefix,
	 (i == SCHED_OTHER) ? "SCHED_OTHER" :
	 (i == SCHED_FIFO)  ? "SCHED_FIFO" :
	 (i == SCHED_RR)    ? "SCHED_RR" :
	 "???");

  s = pthread_attr_getschedparam(&attr, &sp);
  if (s)
    errdie("pthread_attr_getschedparam");
  printf("%sScheduling priority = %d\n", prefix, sp.sched_priority);

  s = pthread_attr_getguardsize(&attr, &v);
  if (s != 0)
    errdie("pthread_attr_getguardsize");
  printf("%sGuard size          = %lu bytes\n", prefix, v);

  s = pthread_attr_getstack(&attr, &stkaddr, &v);
  if (s != 0)
    errdie("pthread_attr_getstack");
  printf("%sStack address       = %p\n", prefix, stkaddr);
  printf("%sStack size          = 0x%zx bytes\n", prefix, v);
  pthread_mutex_unlock(&showinfo_mutex);  
}

TimingStruct trialTimer;

void
startThreadTimer(int tid) {
  if (tid == 0) {
    myBarrier(&loopBarrier);
    startTimer(&trialTimer);
  } else {
    myBarrier(&loopBarrier);
  }
}

nanoseconds
endThreadTimer(int tid) {
  nanoseconds duration = 0;

  if (tid == 0) {
    myBarrier(&loopBarrier);
    duration = endTimer(&trialTimer);
  } else {
    myBarrier(&loopBarrier);
  }
  return duration;
}

////////////////////////////////////////////////////////////////
// probability distribution

long int
getVal(void)
{
  return random();
}

void
insertTrial(HashTable* table, int n) {
  for (int i=0; i<n; i++) {
    long int val = getVal();
    insertTable(table, val);
  }
}

////////////////////////////////////////////////////////////////
// main thread function: run

void*
run(void* arg) {
  int tid = (int)(long int)arg;
  threadId = tid;
  if (verbose) fprintf(stderr, "Starting thread: %d\n", tid);

  // if showing thread info, do so now
  if (showthreadattr) showThreadInfo();

  int nipt = numInsertions/nthreads;
  
  notDone = 1;
  do {
    // allocate hash table
    HashTable* table = createTable(hsize);
    startThreadTimer(tid);
    // run trial

    insertTrial(table, numInsertions);
    
    // end trial
    nanoseconds ns = endThreadTimer(tid);
    // record time and see if we are done
    if (tid == 0) {
      printf("%2d %9llu\n", trialNumber, ns);
      trialTimes[trialNumber++] = ns;
      if ((stopError != 0)&&(trialNumber > trialsToRun)) {
	double median = getMedian(trialTimes, trialNumber);
	double mean = getMean(trialTimes, trialNumber);
	double stddev = getSD(trialTimes, trialNumber);
	if (stddev <= stopError) notDone = 0;
	else if (trialNumber > 10*trialsToRun) notDone = 0;
	if (verbose) printf("Med:%lf, Avg:%lf, SD:%lf\n", median, mean, stddev);
      } else if (trialNumber > trialsToRun) {
	notDone = 0;
      } 
    }
    myBarrier(&endLoopBarrier);
    // free table
    freeTable(table);
    
  } while (notDone);

  // when all done, let main thread know
  semPost(&threadsDone);
}

////////////////////////////////////////////////////////////////
// main entry point

void
main(int argc, char**argv)
{
  progname = argv[0];
  randomSeed = time(NULL);
  int ok = parseArgs(argc, argv, &argp);
  if (ok) die("Error parsing arguments");

  // setup to track different trials
  if (stopError > 0.0) {
    if (trialsToRun == 0) trialsToRun = 5;
  } else if (trialsToRun == 0) {
    trialsToRun = 1;
  }
  trialTimes = calloc(trialsToRun*((stopError > 0)?10:1), sizeof(nanoseconds));

  //creating num_threads threads to add items in parallel
  //see run function for more details
  int numcores=sysconf(_SC_NPROCESSORS_ONLN);
  threadids = calloc(nthreads, sizeof(pthread_t));
  int result = sem_init(&threadsDone, 0, 0);
  if (result) errdie("Can't init threadsDone Sem");

  // setup various barriers
  initBarrier(&loopBarrier);
  initBarrier(&endLoopBarrier);

  // start threads
  for(int i =0; i<nthreads; i++) {
    pthread_attr_t attr;
    result = pthread_attr_init(&attr);
    if (result) errdie("Can't do attr_init");

    // allocate each thread on its own core
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(i%numcores, &cpuset);
    
    result = pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset);
    if (result) die("setaffinitity fails: %d", result);
    result = pthread_create(&threadids[i], &attr, run, (void*)(long int)i);
    if (result) errdie("Can't create threads");
  }

  // threads are all launched, now wait til they are all done
  semWait(&threadsDone);

  // clean them up
  for(int i=0;i<nthreads;i++){
    pthread_join(threadids[i], NULL);
  }

  // exit and print out any results
  double min = getMin(trialTimes, trialNumber);
  double max = getMax(trialTimes, trialNumber);
  printf("Min:%lf, Max:%lf, Range:%lf, Avg:%lf, Median:%lf, SD:%lf\n",
	 min, max, max-min, 
	 getMean(trialTimes, trialNumber), 
	 getMedian(trialTimes, trialNumber), 
	 getSD(trialTimes, trialNumber));
}
