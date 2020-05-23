#ifndef PATPROB_INCLUDED
#  define PATPROB_INCLUDED

#include "typedefs.h"
#include <gsl/gsl_rng.h>

BranchTab *patprob(const void *network, long nreps, int doSing,
                   gsl_rng *rng);
#endif
