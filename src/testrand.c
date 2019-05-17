#include <stdio.h>
#include "dist.h"

#define NumBuckets 20

int
main (int argc, char** argv)
{
  int n = atoi(argv[1]);
  double alpha = atof(argv[2]);
  double beta =  atof(argv[3]);
  int buckets[NumBuckets] = {0};

  /* create a generator chosen by the
     environment variable GSL_RNG_TYPE */

  initProbDist();
  ProbDist* pd = makeRandGenerator(alpha, beta);

  for (int i = 0; i < n; i++) {
    int bucket = getRandLong(pd, NumBuckets-1);
      buckets[bucket]++;
    }

  int scale = 0;
  for (int i=0; i<NumBuckets; i++) {
    if (buckets[i] > scale) scale = buckets[i];
  }
  double factor = 60.0/(double)scale;
  for (int i=0; i<NumBuckets; i++) {
    for (int x=0; x<(int)(buckets[i]*factor); x++) putchar('*');
    putchar('\n');
  }
  freeRandGenerator(pd);
  return 0;
}
