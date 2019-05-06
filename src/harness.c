#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include "arg.h"

const char* progname;
#define Version "0.1"

static void
die(const char* fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    fprintf(stderr, "%s: Usage Error: ", progname);
    vfprintf(stderr, fmt, ap);
    va_end (ap);
    exit(-1);
}

////////////////////////////////////////////////////////////////
// arguments, description, version, etc.

int verbose = 0;
int level = 0;
int quiet = 0;
int timeit = 0;
int randomSeed = 0;
int nthreads = 1;

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
  { KindOption, Set, 		"-v", 0, &verbose, 	"Turn on verbosity" },
  { KindOption, Integer, 	"-l", 0, &level, 	"Level" },
  { KindOption, Set, 		"-q", 0, &quiet, 	"Run Silently - no output (useful for timing)" },    
  { KindOption, Set, 		"-T", 0, &timeit, 	"Time the run" },    
  { KindOption, Function, 	"-r", 0, &setRandom, 	"Set random seed (otherwise uses time)" },    
  { KindOption, Integer, 	"-t", 0, &nthreads, 	"Number of threads" },
  { KindHelp,   Help, 		"-h" },  
  { KindEnd }
};
static ArgDefs argp = { args, "Harness for parallel hashing", Version };

////////////////////////////////////////////////////////////////
// basic helper functions

#define mycalloc(x, y) myCalloc((x), (y), __FILE__, __LINE__)
void*
myCalloc(size_t nmemb, size_t size, const char* fname, const int ln)
{
  void* p = calloc(nmemb, size);
  if (p == NULL) die("Failed to allocate memory at %s:%d", fname, ln);
  return p;
}

void
main(int argc, char**argv)
{
  progname = argv[0];
  randomSeed = time(NULL);
  int ok = parseArgs(argc, argv, &argp);
  printf("%d <- %d, %d, %d, %d, %d, %d\n", ok, verbose, level, quiet, timeit, randomSeed, nthreads);
}
