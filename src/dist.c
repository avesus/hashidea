#include "dist.h"
#include "util.h"
#include "math.h"

static const gsl_rng_type * T;

// initialize prob dists, called once from main
void
initProbDist(void)
{
  gsl_rng_env_setup();
  T = gsl_rng_default;
}

// destory and free memory
void 
freeRandGenerator(ProbDist* pd)
{
  gsl_rng_free(pd->r);
  free(pd);
}

// create an alpha-beta prob dist for a particular thread
ProbDist* 
makeRandGenerator(double a, double b)
{
  ProbDist* pd = mycalloc(1, sizeof(ProbDist));
  pd->alpha = a;
  pd->beta = b;
  pd->r = gsl_rng_alloc(T);
  return pd;
}

long int 
getRandLong(ProbDist* p, long range)
{
  return (long int)round((double)range * gsl_ran_beta(p->r, p->alpha, p->beta));
}

