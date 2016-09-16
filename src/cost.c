/**
   @file cost.c
   @page cost
   @brief Calculate cost function

   @copyright Copyright (c) 2016, Alan R. Rogers 
   <rogers@anthro.utah.edu>. This file is released under the Internet
   Systems Consortium License, which can be found in file "LICENSE".
*/

#undef DEBUG
#undef DPRINTF_ON

#include "dprintf.h"
#ifdef DPRINTF_ON
#include <pthread.h>
extern pthread_mutex_t outputLock;
#endif

#include "cost.h"
#include "gptree.h"
#include "branchtab.h"
#include "patprob.h"
#include "misc.h"
#include <math.h>
#include <gsl/gsl_rng.h>

/// @param[in] x vector of parameter values.
double costFun(int dim, double x[dim], void *jdata, void *tdata) {
    CostPar *cp = (CostPar *) jdata;
    gsl_rng *rng = (gsl_rng *) tdata;

	GPTree_setParams(cp->gptree, dim, x);
	if(!GPTree_feasible(cp->gptree)) 
		return HUGE_VAL;

    BranchTab  *prob = patprob(cp->gptree, cp->nThreads, cp->nreps,
                               cp->doSing, rng);
    BranchTab_divideBy(prob, cp->nreps);
#if COST==KL_COST
    BranchTab_normalize(prob);
    double cost = BranchTab_KLdiverg(cp->obs, prob);
#elif COST==CHISQR_COST   
    double cost = BranchTab_chiSqCost(cp->obs, prob, cp->u, cp->nnuc,
                                      cp->nreps);
#elif COST==SMPLCHISQR_COST   
    double cost = BranchTab_smplChiSqCost(cp->obs, prob, cp->u, cp->nnuc,
                                      cp->nreps);
#elif COST==POISSON_COST   
    double cost = BranchTab_poissonCost(cp->obs, prob, cp->u, cp->nnuc,
                                        cp->nreps);
#else
# error "Unknown cost method"
#endif
    
    return cost;
}

void * CostPar_dup(const void * arg) {
    assert(arg);
    const CostPar *old = (const CostPar *) arg;
    if(arg==NULL)
        return NULL;
    CostPar *new = memdup(old, sizeof(CostPar));
    CHECKMEM(new);
    new->obs = BranchTab_dup(old->obs);
    CHECKMEM(new->obs);
    new->gptree = GPTree_dup(old->gptree);
    CHECKMEM(new->gptree);
    return new;
}

void CostPar_free(void *arg) {
    if(arg)
        free(arg);
}
