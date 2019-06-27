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
#define min(X, Y)  ((X) < (Y) ? (X) : (Y))
#define max(X, Y)  ((X) < (Y) ? (Y) : (X))

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

int coolOff = 0;
int showArgs = 0;
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

int regtemp = 0;
int tracktemp=0;
double AllowedTempVariance=1.1;

int trialNumber = 0;

PerTrialInfo* trialData;	/* all the data for every trial */
PerTrialInfo* tdp;		/* pointer to PerTrialInfo for this time tick */

// make sure we are setup to collect data for this trial.  Sets global
// tdp, assumes trialNumber is set
void
setupToCollectTrialStats(int tid) {
  if (tid != 0) return;
  tdp = trialData + trialNumber;
  tdp->barrierEnds = calloc(nthreads, sizeof(nanoseconds));
  if (tracktemp||regtemp) {
    tdp->startTemp = calloc(nthreads, sizeof(double));
    tdp->endTemp = calloc(nthreads, sizeof(double));
  }
  //fprintf(stderr, "stcts:%d tdp:%p, tdp->BE:%p, tdp:ST:%p, tdp:ET:%p\n", tid, tdp, tdp->barrierEnds, tdp->startTemp, tdp->endTemp);
}

typedef struct targs {
  unsigned int* seeds;
  int tid;
} targs;

double alpha = 0.5;
double beta = 0.5;

const char*
showVersion(int argc, char** argv)
{
  static char buffer[64];
  if (argc == ArgGetDefault) {
    return "";
  } else if (argc == ArgGetDesc) {
    return "";
  }
  const char* ws =
#if COLLECT_STAT==1
    ",\t(with Statistics)";
#else
  "";
#endif    
    printf("%s: %s aka %s%s\n", progname, getProgramPrefix(), getProgramShortPrefix(), ws);
  exit(0);
}

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
  { KindOption,   Integer,	"-c", 		0, &coolOff, 		"seconds to sleep between harness runs" },
  { KindOption,   Set, 		"-vt", 		0, &showthreadattr, 	"Turn on verbosity" },
  { KindOption,   Integer, 	"-l", 		0, &level, 		"Level" },
  { KindOption,   Integer, 	"--inserts",	0, &numInsertions,	"total number of insertions" },
  { KindOption,   Function, 	"--version",	0, &showVersion,	"show current version" },
  { KindOption,   Set,	 	"--args",	0, &showArgs,		"show all args on stdout" },
  { KindOption,   Integer, 	"--trials", 	0, &trialsToRun,	"Number of trials to run (0 means 1 or use --terror" },
  { KindOption,   Double, 	"--terror", 	0, &stopError, 		"Run trials til get to error below this (0 means use --trials)" },
  { KindOption,   Set, 		"-q", 		0, &quiet, 		"Run Silently - no output (useful for timing)" },    
  { KindOption,   Set, 		"-T", 		0, &timeit, 		"Time the run" },    
  { KindOption,   Function,	"--ab",		0, &initAlphaBeta, 	"default alpha beta for keys" },
  { KindOption,   Double,	"--qp",		0, &queryPercentage, 	"Percent of actions that are queries" },
  { KindOption,   Function, 	"-r", 		0, &setRandom, 		"Set random seed (otherwise uses time)" },    
  { KindOption,   Set	, 	"--check",	0, &checkT, 		"check table is working correctly" },    
#if COLLECT_STAT==1
  { KindOption,   Set	, 	"--ts",		0, &statspertrial,	"show stats after each trial" },    
#endif
  { KindOption,   Integer, 	"-t", 		0, &nthreads, 		"Number of threads" },
  { KindHelp,     Help, 	"-h" },
  { KindOption,   Integer,      "-a",           0, &HashAttempts,       "Set hash attempts for open table hashing" },
  { KindOption,   Integer,      "-i",           0, &InitSize,           "Set table size for starting table" },
  { KindOption,   Set,          "--regtemp",    0, &regtemp,            "Set so that between trials will wait until cpu temp returns to close to starting temp." },
  { KindOption,   Set,          "--tracktemp",  0, &tracktemp,          "Track thread temps for each trial. Note this will affect timing slightly" },
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

BarrierSummary* trialBTS;
BarrierSummary* barrierTimesPointer;

void
calcBSmedian(BarrierSummary* bts, int n, double* tomin, double* tomed)
{
  double min = 0;
  double med = 0;
  for (int i=0; i<n; i++) {
    min += bts->maxgap;
    med += bts->medgap;
  }
  *tomin = min/(double)n;
  *tomed = med/(double)n;
}


// used to keep track of each trial, used by only thread 0.
static TimingStruct trialTimer;

void
startThreadTimer(int tid, int trialNum) 
{
  if (tid == 0) {
    if (regtemp) enforceTemps(nthreads);	/* enforce temperature if asked */
    if (tracktemp) doTemps(0, statAdrOf(tdp, startTemp, double), nthreads);
    myBarrier(&loopBarrier, tid);
    startTimer(&trialTimer);
  } else {
    myBarrier(&loopBarrier, tid);
  }
}

nanoseconds
endThreadTimer(int tid, int trialNum) {
  nanoseconds duration = 0;
  if(tracktemp) {
    doTemps(tid, statAdrOf(tdp, endTemp, double), 1);
    printPTI(stdout, tdp);
  }

  if (tid == 0) {
    myBarrier(&loopBarrier, tid);
    duration = endTimer(&trialTimer);
    if (verbose) showWaiting(&loopBarrier, "ETT");
    getBTsummary(&loopBarrier, tdp->barrierEnds, &tdp->barrierGaps);
    if (verbose) printf("BT Summary: max:%g\tmedian:%g\n", tdp->barrierGaps.maxgap, tdp->barrierGaps.medgap);
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
      //        deleteVal(head, val);
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



#if COLLECT_STAT==1
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
    setupToCollectTrialStats(tid);
    
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
    startThreadTimer(tid, trialNumber);

    if (checkT) {
      // run check
      checkTable(globalHead, numInsertions, tid);
    } else {
      // run trial
      insertTrial(globalHead, numInsertions, tid, entChunk, rVals);
    }
    
    // end timer
    nanoseconds ns = endThreadTimer(tid, trialNumber);

    // record time and see if we are done
    if (tid == 0) {
      if (verbose) printf("%2d %9llu %d %d\n", trialNumber, ns, tid, threadId);
      if(verbose&&tracktemp){
	printTempsV(tdp, nthreads);
      }
      tdp->time = ns;
      if ((stopError != 0)&&(trialNumber+1 > trialsToRun)) {
	double median = getMedianL(tdp, statOffset(time), trialNumber);
	double mean = getMeanL(tdp, statOffset(time), trialNumber);
	double stddev = getSDL(tdp, statOffset(time), trialNumber);
      
	if (stddev <= stopError) notDone = 0;
	else if ((trialNumber+1) >= 10*trialsToRun) notDone = 0;
	if (verbose) printf("Med:%lf, Avg:%lf, SD:%lf\n", median, mean, stddev);
      } else if ((trialNumber+1) >= trialsToRun) {
	notDone = 0;
      }

      tdp->memutils= freeAll(globalHead, !notDone, verbose);
      if (statspertrial) {
	printStats();
	clearStats();
      }
      trialNumber++;
    }
    free(entChunk);
    free(rVals);
    sleep(coolOff);
    // make sure we start the loop anew at the same basic time
    myBarrier(&endLoopBarrier, tid);
  } while (notDone);

  // when all done, let main thread know
  semPost(&threadsDone);
}

static char desc[256];

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


  
  if(tracktemp||regtemp){
    if(initTemp(trialsToRun, nthreads)){
      printf("Error accessing CPU temp/cores\n");
      return;
    }
    if(regtemp){
      setEnforcedTemps(AllowedTempVariance, nthreads);
    }
  }
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
  trialData = calloc(trialsToRun*((stopError > 0)?10:1), sizeof(PerTrialInfo));
  tdp = trialData;

  // allocate stats if compiled in
  clearStats();
  
  if (showArgs) {
    printf("GPP,SP,numInsertions, trialsToRun, stopError, alpha, beta, queryPercentage, randomSeed, nthreads,HashAttempts,InitSize,cooloff,enfTemp,tempvar,HEADING\n");
    // if we are asked to show all args, print them out here one one line
    sprintf(desc, "%s,%s,%d,%d,%lf,%lf,%lf,%lf,%d,%d,%d,%d,%d,%d,%lf", 
	    getProgramPrefix(), getProgramShortPrefix(),
	   numInsertions, trialsToRun, stopError, alpha, beta, 
	    queryPercentage, randomSeed, nthreads,HashAttempts,InitSize,coolOff,regtemp,AllowedTempVariance);
    printf("%s,START\n", desc);
  } else {
    // just show vital ones
    printf("%s\t%s\tthreads:%d\tquery-percent:%f\n", getProgramPrefix(), getProgramShortPrefix(), nthreads, queryPercentage);
    strcpy(desc, getProgramShortPrefix());
  }

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
  double min = getMinL(trialData, statOffset(time), trialNumber);
  double max = getMaxL(trialData, statOffset(time), trialNumber);
  printf("%s,\tMilliseconds,\tMin:%lf, Max:%lf, Range:%lf, Avg:%lf, Median:%lf, SD:%lf",
	 desc,
	 min/1000000.0, max/1000000.0, (max-min)/1000000.0, 
	 getMeanL(trialData, statOffset(time), trialNumber)/1000000.0, 
	 getMedianL(trialData, statOffset(time), trialNumber)/1000000.0, 
	 getSDL(trialData, statOffset(time), trialNumber)/1000000.0);
  if (trialNumber > 4) {
    nanoseconds* trimmedTimes = mycalloc(trialNumber, sizeof(sizeof(nanoseconds)));
    int n = trialNumber-2;
    trimData(trialNumber, statOffset(time), trialData, trimmedTimes);
    min = getMin(trimmedTimes, n);
    max = getMax(trimmedTimes, n);
    printf(",\tTMin:%lf, TMax:%lf, TRange:%lf, TAvg:%lf, TMedian:%lf, TSD:%lf", 
	 min/1000000.0, max/1000000.0, (max-min)/1000000.0, 
	 getMean(trimmedTimes, n)/1000000.0, 
	 getMedian(trimmedTimes, n)/1000000.0, 
	 getSD(trimmedTimes, n)/1000000.0);
  }
  printf("\n");

  min = getMinFloat(trialData, statOffset(memutils), trialNumber);
  max = getMaxFloat(trialData, statOffset(memutils), trialNumber);
  printf("%s,\tMemusage,\tMin:%lf, Max:%lf, Range:%lf, Avg:%lf, Median:%lf, SD:%lf\n",
	 desc,
	 min, max, max-min, 
	 getMeanFloat(trialData, statOffset(memutils), trialNumber), 
	 getMedianFloat(trialData, statOffset(memutils), trialNumber), 
	 getSDFloat(trialData, statOffset(memutils), trialNumber));
  double tomingap = getMedianFloat(trialData, statOffset(barrierGaps), trialNumber);
  double tomedgap = getMedianFloat(trialData, statOffset(barrierGaps)+sizeof(double), trialNumber);
  printf("%s,\tBarrier Gap,\tMax->Min:%lf, Max->Median:%lf\n",
	 desc,
	 tomingap,
	 tomedgap);
  if(tracktemp) printTempsResults(desc, nthreads, trialNumber, showArgs);

  if (showArgs) {
    printf("%s,END\n", desc);
  }

  printStats();
  freeCommandLine();
  freeArgumentParser(ap);
}
