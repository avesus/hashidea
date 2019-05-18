#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>

#ifndef _LL_HTABLE_H_
#define _LL_HTABLE_H_

typedef struct pointer{
  volatile struct node* ptr;
}pointer;

typedef struct node{
  volatile unsigned long val;
  volatile struct pointer next;
}node;

typedef struct que{
  volatile struct pointer head;
  volatile struct pointer tail;
}que;

typedef struct table{
  que ** q;
  int size;
}table;

typedef struct g_head{
  table** t;
  unsigned int seed;
  int cur;
  int bucketMax;
}g_head;
