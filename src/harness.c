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
#include "../lib/hashtable.h"
#include "../lib/hashstat.h"
#include "dist.h"

#define Version "0.1"

static char*
getProgramPrefix(void) {
  static char* prefix = NULL;
  if (prefix == NULL) {
    prefix = mycalloc(strlen(tablename)+16, 1);
    sprintf(prefix, "%s:harness:%s", tablename, Version);
  }
  return prefix;
}

static char*
getProgramShortPrefix(void) {
  static char* prefix = NULL;
  if (prefix == NULL) {
    prefix = mycalloc(strlen(shortname)+16, 1);
    sprintf(prefix, "%s/h:%s", shortname, Version);
  }
  return prefix;
}

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
int InitSize = 1;
int HashAttempts = 1;
HashTable* globalHead=NULL;
double queryPercentage = 0.0;
int queryCutoff = 0;
int checkT = 0;
int statspertrial = 0;

int trialNumber = 0;
nanoseconds* trialTimes;
double* trialUtils;

typedef struct targs{
  unsigned int* seeds;
  int tid;


}targs;

double alpha = 0.5;
double beta = 0.5;

const char*
initAlphaBeta(int argc, char** argv)
{
  static char buffer[64];
  if (argc == ArgGetDefault) {
    sprintf(buffer, "%lf,%lf", alpha, beta);
    return buffer;
  } else if (argc == ArgGetDesc) {
    return "<alpha>,<beta>";
  }
  if (sscanf(argv[1], "%lf,%lf", &alpha, &beta) != 2) {
    errdie("expected 2 floats, e.g., <alpha>,<beta>");
  }
  return (const char*)1;
}

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
  // Kind, 	  Method,		name,	    reqd,  variable,		help
  { KindOption,   Set, 		"-v", 		0, &verbose, 		"Turn on verbosity" },
  { KindOption,   Set, 		"-vt", 		0, &showthreadattr, 	"Turn on verbosity" },
  { KindOption,   Integer, 	"-l", 		0, &level, 		"Level" },
  { KindOption,   Integer, 	"--inserts",	0, &numInsertions,	"total number of insertions" },
  { KindOption,   Integer, 	"--trials", 	0, &trialsToRun,	"Number of trials to run (0 means 1 or use --terror" },
  { KindOption,   Double, 	"--terror", 	0, &stopError, 		"Run trials til get to error below this (0 means use --trials)" },
  { KindOption,   Set, 		"-q", 		0, &quiet, 		"Run Silently - no output (useful for timing)" },    
  { KindOption,   Set, 		"-T", 		0, &timeit, 		"Time the run" },    
  { KindOption,   Function,	"--ab",		0, &initAlphaBeta, 	"default alpha beta for keys" },
  { KindOption,   Double,	"--qp",		0, &queryPercentage, 	"Percent of actions that are queries" },
  { KindOption,   Function, 	"-r", 		0, &setRandom, 		"Set random seed (otherwise uses time)" },    
  { KindOption,   Set	, 	"--check",	0, &checkT, 		"check table is working correctly" },    
#ifdef COLLECT_STAT
  { KindOption,   Set	, 	"--ts",		0, &statspertrial,	"show stats after each trial" },    
#endif
  { KindOption,   Integer, 	"-t", 		0, &nthreads, 		"Number of threads" },
  { KindHelp,     Help, 	"-h" },
  { KindOption,   Integer,      "-a",           0, &HashAttempts,       "Set hash attempts for open table hashing" },
  { KindOption,   Integer,      "-i",           0, &InitSize,           "Set table size for starting table" },
  { KindEnd }
};
static ArgDefs argp = { args, "Harness for parallel hashing", Version, NULL };

pthread_t* threadids;		/* array of thread ids */
sem_t threadsDone;		/* used to signal when all threads are done */
volatile int notDone = 1;		/* set to 0 when we are all done */
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
    myBarrier(&loopBarrier, tid);
    startTimer(&trialTimer);
  } else {
    myBarrier(&loopBarrier, tid);
  }
}

nanoseconds
endThreadTimer(int tid) {
  nanoseconds duration = 0;

  if (tid == 0) {
    myBarrier(&loopBarrier, tid);
    duration = endTimer(&trialTimer);
    showWaiting(&loopBarrier, "ETT");
  } else {
    myBarrier(&loopBarrier, tid);
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

int
isQuery(void)
{
  if ((queryPercentage > 0) && (random() > queryCutoff)) return 1;
  return 0;
}


//initial seeds for multiple hash functions
static unsigned int*
initSeeds(int HashAttempts){
  unsigned int * seeds=(unsigned int*)malloc(sizeof(unsigned int)*HashAttempts);
  for(int i =0;i<HashAttempts;i++){
    seeds[i]=random();
  }
  return seeds;
}


void
insertTrial(HashTable* head, int n, int tid, void* entChunk, unsigned long* rVals) {
  for (int i=0; i<n; i++) {
    unsigned long val = rVals[i];

    if ((queryPercentage > 0) && (rVals[numInsertions+i] > queryCutoff)){
      checkTableQuery(head, val);
    }
    else{
      entry* ent = (entry*)(entChunk+(i<<4));  
    ent->val = val;

    insertTable(head, getStart(head), ent, tid);
    }
  }
  //  free(entChunk);
}

void
checkTable(HashTable* head, int n, int tid) {
  // insert n keys from all threads
  for (int i=0; i<n; i++) {
    entry* ent=(entry*)malloc(sizeof(entry));
    ent->val = i;
    insertTable(head, getStart(head), ent, tid);
  }
  // see if they are all there
  for (int i=0; i<n; i++) {
    if (checkTableQuery(head, i) != 1) {
      printf("%d: failed to find %d\n", tid, i);
    }
  }
}



#ifdef COLLECT_STAT
////////////////////////////////////////////////////////////////
// for stats

StatPrintInfo statsinfo[] = {
  statline(checktable_outer),
  statline(checktable_hashatmpts),
  statline(lookup_copy),
  statline(adddrop),
  statline(addrop_fail),
  statline(inserttable_outer),
  statline(inserttable_hashatmpts),
  statline(inserttable_inserts),
  statline(createtable),
  { NULL, 0 }
};

StatInfo* stats;

void
printStats(void) {
  for (int j=0; statsinfo[j].name != NULL; j++) {
    printf("%30s:", statsinfo[j].name);
    for (int x=0; x<nthreads; x++) {
      printf("\t%6d", *(int *)( ((char*)(stats+x)+statsinfo[j].offset) ));
    }
    printf("\n");
  }
}

void
clearStats(void) {
  if (stats) {
    memset(stats, 0, sizeof(StatInfo)*nthreads);
  } else {
    stats = calloc(nthreads, sizeof(StatInfo));
  }
}
#else
# define printStats()
# define clearStats()
#endif


////////////////////////////////////////////////////////////////
// main thread function: run

void*
run(void* arg) {
  targs* args=(targs*)arg;
  int tid=args->tid;
  unsigned int* seeds=args->seeds;
  free(args);
  
  threadId = tid;
  if (verbose) fprintf(stderr, "Starting thread: %d\n", tid);

  // if showing thread info, do so now
  if (showthreadattr) showThreadInfo();

  notDone = 1;
  

  do {
    unsigned long* rVals=(unsigned long*)malloc(2*sizeof(unsigned long)*numInsertions);
    for(int i =0;i<numInsertions;i++){
      rVals[i]=random();
      rVals[i]+=!rVals[i];
    }
    for(int i =numInsertions;i<2*numInsertions;i++){
      rVals[i]=random();
    }
    
    void* entChunk=malloc(numInsertions*sizeof(entry)*2);
    //start timer
    if(!tid){
      globalHead=initTable(globalHead, InitSize, HashAttempts, nthreads, seeds);

    }
    startThreadTimer(tid);

    if (checkT) {
      // run check
      checkTable(globalHead, numInsertions, tid);
    } else {
      // run trial
      insertTrial(globalHead, numInsertions, tid, entChunk, rVals);
    }
    
    // end timer
    nanoseconds ns = endThreadTimer(tid);

    // record time and see if we are done
    if (tid == 0) {

      if (verbose) printf("%2d %9llu %d %d\n", trialNumber, ns, tid, threadId);
      trialTimes[trialNumber] = ns;
      if ((stopError != 0)&&(trialNumber+1 > trialsToRun)) {
	double median = getMedian(trialTimes, trialNumber);
	double mean = getMean(trialTimes, trialNumber);
	double stddev = getSD(trialTimes, trialNumber);
	if (stddev <= stopError) notDone = 0;
	else if ((trialNumber+1) >= 10*trialsToRun) notDone = 0;
	if (verbose) printf("Med:%lf, Avg:%lf, SD:%lf\n", median, mean, stddev);
      } else if ((trialNumber+1) >= trialsToRun) {
	notDone = 0;
      }

      trialUtils[trialNumber]= freeAll(globalHead, !notDone, verbose);
      trialNumber++;
      if (statspertrial) {
	printStats();
	clearStats();
      }
    }

    myBarrier(&endLoopBarrier, tid);
        free(entChunk);
	free(rVals);
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
  srand(randomSeed);
  ArgParser* ap = createArgumentParser(&argp);
  addArgumentParser(ap, getProbDistArgParsing(), 0);
  int ok = parseArguments(ap, argc, argv);
  if (ok) die("Error parsing arguments");

  // decide on query breakdown
  if (queryPercentage > 0) {
    queryCutoff = (long int)((1.0-queryPercentage)*(double)RAND_MAX);
  }

  // setup to track different trials
  if (stopError > 0.0) {
    if (trialsToRun == 0) trialsToRun = 5;
  } else if (trialsToRun == 0) {
    trialsToRun = 1;
  }
  trialTimes = calloc(trialsToRun*((stopError > 0)?10:1), sizeof(nanoseconds));
  trialUtils = calloc(trialsToRun*((stopError > 0)?10:1), sizeof(double));

  // allocate stats if compiled in
  clearStats();
  
  printf("%s\t%s\t%d\t%f\n", getProgramPrefix(), getProgramShortPrefix(), nthreads, queryPercentage);

  //creating num_threads threads to add items in parallel
  //see run function for more details
  int numcores=sysconf(_SC_NPROCESSORS_ONLN);
  threadids = calloc(nthreads, sizeof(pthread_t));
  int result = sem_init(&threadsDone, 0, 0);
  if (result) errdie("Can't init threadsDone Sem");

  // setup various barriers
  initBarrier(&loopBarrier);
  initBarrier(&endLoopBarrier);

  //init hash functions
  unsigned int* seeds=initSeeds(HashAttempts);
  
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
    
    targs* temp_args=malloc(sizeof(targs));
    temp_args->seeds=seeds;
    temp_args->tid=i;
    
    result = pthread_create(&threadids[i], &attr, run, (void*)temp_args);
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
#if 0 
  printf("Nanoseconds::\n Min:%lf, Max:%lf, Range:%lf, Avg:%lf, Median:%lf, SD:%lf\n",
	 min, max, max-min, 
	 getMean(trialTimes, trialNumber), 
	 getMedian(trialTimes, trialNumber), 
	 getSD(trialTimes, trialNumber));
#endif
  printf("%s:Milliseconds:\tMin:%lf, Max:%lf, Range:%lf, Avg:%lf, Median:%lf, SD:%lf",
	 getProgramShortPrefix(),
	 min/1000000.0, max/1000000.0, (max-min)/1000000.0, 
	 getMean(trialTimes, trialNumber)/1000000.0, 
	 getMedian(trialTimes, trialNumber)/1000000.0, 
	 getSD(trialTimes, trialNumber)/1000000.0);
  if (trialNumber > 4) {
    nanoseconds* trimmedTimes = mycalloc(trialNumber, sizeof(sizeof(nanoseconds)));
    int n = trialNumber-2;
    trimData(trialNumber, trialTimes, trimmedTimes);
    min = getMin(trimmedTimes, n);
    max = getMax(trimmedTimes, n);
    printf("\tMin:%lf, Max:%lf, Range:%lf, Avg:%lf, Median:%lf, SD:%lf", 
	 min/1000000.0, max/1000000.0, (max-min)/1000000.0, 
	 getMean(trimmedTimes, n)/1000000.0, 
	 getMedian(trimmedTimes, n)/1000000.0, 
	 getSD(trimmedTimes, n)/1000000.0);
  }
  printf("\n");

  min = getMinFloat(trialUtils, trialNumber);
  max = getMaxFloat(trialUtils, trialNumber);
  printf("%s:Memusage:\tMin:%lf, Max:%lf, Range:%lf, Avg:%lf, Median:%lf, SD:%lf\n",
	 getProgramShortPrefix(),
	 min, max, max-min, 
	 getMeanFloat(trialUtils, trialNumber), 
	 getMedianFloat(trialUtils, trialNumber), 
	 getSDFloat(trialUtils, trialNumber));

  printStats();
  freeCommandLine();
  freeArgumentParser(ap);
}
