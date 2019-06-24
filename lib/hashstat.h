#ifndef _HASHSTAT_H_
#define _HASHSTAT_H_

// define when you make
//#define COLLECT_STAT 1

#include <stddef.h>

typedef struct {
  int checktable_outer;
  int checktable_hashatmpts;
  int lookup_copy;
  int adddrop;
  int inserttable_outer;
  int inserttable_hashatmpts;
  int inserttable_inserts;
  int createtable;
  int addrop_fail;
} StatInfo;

extern StatInfo* stats;

#if COLLECT_STAT==1
#define IncrStat(x)	do { stats[threadId].x++; } while (0)
#else
#define IncrStat(x)
#endif

////////////////////////////////////////////////////////////////
// for printing stats

typedef struct {
  const char* name;
  int offset;
} StatPrintInfo;

#define statline(x)	{ #x, offsetof(StatInfo, x) }

#endif
