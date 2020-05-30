/**
 * @file segment.c
 * @author Alan R. Rogers
 * @brief A single segment of a population tree.
 *
 * @copyright Copyright (c) 2020, Alan R. Rogers
 * <rogers@anthro.utah.edu>. This file is released under the Internet
 * Systems Consortium License, which can be found in file "LICENSE".
 */

#include "binary.h"
#include "branchtab.h"
#include "comb.h"
#include "error.h"
#include "idset.h"
#include "intpart.h"
#include "matcoal.h"
#include "misc.h"
#include "nodestore.h"
#include "partprob.h"
#include "segment.h"
#include <stdlib.h>
#include <string.h>

typedef struct CombDat CombDat;

struct CombDat {
    double contribution; // Pr[site pattern]*E[len of interval]
    IdSet *ids;
    BranchTab *branchtab;
    int dosing;    // do singleton site patterns if nonzero
};

void   Segment_addIdSet(Segment *self, IdSet *idset);
static void Segment_sanityCheck(Segment * self, const char *file, int lineno);
int    visitComb(int d, int ndx[d], void *data);

void *Segment_new(double *twoN, double *start, NodeStore * ns) {
    Segment *self = NodeStore_alloc(ns);
    CHECKMEM(self);

    memset(self, 0, sizeof(*self));

    self->twoN = twoN;
    self->start = start;

    return self;
}

int      Segment_addChild(void * vparent, void * vchild) {
    Segment *parent = vparent;
    Segment *child = vchild;
    if(parent->nchildren > 1) {
        fprintf(stderr,
                "%s:%s:%d: Can't add child because parent already has %d.\n",
                __FILE__, __func__, __LINE__, parent->nchildren);
        return TOO_MANY_CHILDREN;
    }
    if(child->nparents > 1) {
        fprintf(stderr,
                "%s:%s:%d: Can't add parent because child already has %d.\n",
                __FILE__, __func__, __LINE__, child->nparents);
        return TOO_MANY_PARENTS;
    }
    if(*child->start > *parent->start) {
        fprintf(stderr,
                "%s:%s:%d: Child start (%lf) must be <= parent start (%lf)\n",
                __FILE__, __func__, __LINE__, *child->start, *parent->start);
        return DATE_MISMATCH;
    }
    if(child->end == NULL) {
        child->end = parent->start;
    } else {
        if(child->end != parent->start) {
            fprintf(stderr, "%s:%s:%d: Date mismatch."
                    " child->end=%p != %p = parent->start\n",
                    __FILE__, __func__, __LINE__, child->end, parent->start);
            return DATE_MISMATCH;
        }
    }
    parent->child[parent->nchildren] = child;
    child->parent[child->nparents] = parent;
    ++parent->nchildren;
    ++child->nparents;
    Segment_sanityCheck(parent, __FILE__, __LINE__);
    Segment_sanityCheck(child, __FILE__, __LINE__);
    return 0;
}

/// Check sanity of Segment
static void Segment_sanityCheck(Segment * self, const char *file, int lineno) {
#ifndef NDEBUG
    REQUIRE(self != NULL, file, lineno);
#endif
}

int      Segment_mix(void * vchild, double *mPtr, void * vintrogressor, 
                     void * vnative) {
    Segment *child = vchild, *introgressor = vintrogressor,
        *native = vnative;

    if(introgressor->nchildren > 1) {
        fprintf(stderr,"%s:%s:%d:"
                " Can't add child because introgressor already has %d.\n",
                __FILE__, __func__, __LINE__, introgressor->nchildren);
        return TOO_MANY_CHILDREN;
    }
    if(native->nchildren > 1) {
        fprintf(stderr,"%s:%s:%d:"
                " Can't add child because native parent already has %d.\n",
                __FILE__, __func__, __LINE__, native->nchildren);
        return TOO_MANY_CHILDREN;
    }
    if(child->nparents > 0) {
        fprintf(stderr, "%s:%s:%d:"
                " Can't add 2 parents because child already has %d.\n",
                __FILE__, __func__, __LINE__, child->nparents);
        return TOO_MANY_PARENTS;
    }
    if(child->end != NULL) {
        if(child->end != introgressor->start) {
            fprintf(stderr,"%s:%s:%d: Date mismatch."
                    " child->end=%p != %p=introgressor->start\n",
                    __FILE__, __func__, __LINE__,
                    child->end, introgressor->start);
            return DATE_MISMATCH;
        }
        if(child->end != native->start) {
            fprintf(stderr, "%s:%s:%d: Date mismatch."
                    " child->end=%p != %p=native->start\n",
                    __FILE__, __func__, __LINE__, child->end, native->start);
            return DATE_MISMATCH;
        }
    } else if(native->start != introgressor->start) {
        fprintf(stderr, "%s:%s:%d: Date mismatch."
                "native->start=%p != %p=introgressor->start\n",
                __FILE__, __func__, __LINE__,
                native->start, introgressor->start);
        return DATE_MISMATCH;
    } else
        child->end = native->start;

    child->parent[0] = native;
    child->parent[1] = introgressor;
    child->nparents = 2;
    child->mix = mPtr;
    introgressor->child[introgressor->nchildren] = child;
    ++introgressor->nchildren;
    native->child[native->nchildren] = child;
    ++native->nchildren;
    Segment_sanityCheck(child, __FILE__, __LINE__);
    Segment_sanityCheck(introgressor, __FILE__, __LINE__);
    Segment_sanityCheck(native, __FILE__, __LINE__);
    return 0;
}

void    *Segment_root(void * vself) {
    Segment *self = vself, *r0, *r1;
    assert(self);
    switch (self->nparents) {
    case 0:
        return self;
        break;
    case 1:
        return Segment_root(self->parent[0]);
        break;
    case 2:
        r0 = Segment_root(self->parent[0]);
        r1 = Segment_root(self->parent[1]);
        if(r0 != r1) {
            fprintf(stderr, "%s:%s:%d: Population network has multiple roots\n",
                    __FILE__, __func__, __LINE__);
            exit(EXIT_FAILURE);
        }
        return r0;
        break;
    default:
        fprintf(stderr, "%s:%s:%d: Node %d parents\n",
                __FILE__, __func__, __LINE__, self->nparents);
        exit(EXIT_FAILURE);
    }
    /* NOTREACHED */
    return NULL;
}

void     Segment_print(FILE * fp, void * vself, int indent) {
    Segment *self = vself;
    for(int i = 0; i < indent; ++i)
        fputs("   ", fp);
    fprintf(fp, "%p twoN=%lf ntrval=(%lf,", self, *self->twoN, *self->start);
    if(self->end != NULL)
        fprintf(fp, "%lf)\n", *self->end);
    else
        fprintf(fp, "Inf)\n");

    for(int i = 0; i < self->nchildren; ++i)
        Segment_print(fp, self->child[i], indent + 1);
}

// Add IdSet to Segment
void Segment_addIdSet(Segment *self, IdSet *idset) {
    int nids = IdSet_nIds(idset);

    assert(nids <= MAXSAMP);

    // add IdSet to segment
    self->ids[0][nids-1] = IdSet_add(self->ids[0][nids-1], idset, 0.0);
}

#if 0
int Segment_coalesce(Segment *self, int maxsamp, int dosing,
                     BranchTab *branchtab, double v) {
    assert(self->max <= maxsamp);

    double pr[maxsamp], elen[maxsamp], sum;
    int n, i, status=0;
    const int finite = isfinite(v); // is segment finite?
    int kstart = (finite ? 1 : 2);  // skip k=1 if infinite

    // If there is only one line of descent, no coalescent events are
    // possible, so p[1][0] is at least as large as p[0][0].
    self->p[1][0] = self->p[0][0];

    // Calculate probabilities and expected values, p[1] and elen.
    for(n=2; n <= self->max; ++n) {

        // Skip improbable states.
        if(self->p[0][n-1] == 0.0)
            continue;
        
        // Calculate prob of 2,3,...,n lines of descent.
        // On return, pr[1] = prob[2], pr[n-1] = prob[n],
        // pr[0] not set.
        MatCoal_project(n-1, pr+1, v);

        // Calculate probability of 1 line of descent. I'm doing the
        // sum in reverse order on the assumption that higher indices
        // will often have smaller probabilities.
        // pr[i] is prob of i+1 lineages; pr[0] not set
        sum=0.0;
        for(i=n; i > 1; --i)
            sum += pr[i-1];
        self->p[1][0] += self->p[0][n-1] * (1.0-sum);

        // Add probs of 2..n lines of descent
        // pr[i] is prob of i+2 lineages
        // self->p[1][i] is prob if i+1 lineages
        for(i=2; i <= n; ++i)
            self->p[1][i-1] += self->p[0][n-1] * pr[i-1];

        // Calculate expected length, within the segment, of
        // coalescent intervals with 2,3,...,n lineages.  elen[i] is
        // expected length of interval with i-1 lineages.
        // elen[0] not set.
        MatCoal_ciLen(n-1, elen+1, v);

        // Calculate expected length, elen[0], of interval with one
        // lineage.  elen[i] refers to interval with i+1 lineages.
        sum = 0.0;
        for(i=n; i >= 2; --i)
            sum += elen[i-1];
        elen[0] = v - sum; // elen[0] is infinite if v is.

        // Multiply elen by probability that segment had n lineages on
        // recent end of segment.
        for(i=1; i <= n; ++i)
            elen[i-1] *= self->p[0][n-1];
    }

    CombDat cd = {.contribution = 0.0,
                  .ids = NULL,
                  .branchtab = branchtab,
                  .dosing = dosing
    };

    // loop over number of descendants in this segment
    for(n=1; n <= self->max; ++n) {
        // Skip improbable states.
        if(self->p[0][n-1] == 0.0)
            continue;

        cd.ids = ids[0][n-1];

        // loop over intervals: k is the number of ancestors
        // w/i interval.
        for(int k=kstart; k <= n; ++k) {
            
            // portion of log Qdk that doesn't involve d
            long double lnconst = logl(k) - lbinom(n-1, k-1);

            // Within each interval, there can be ancestors
            // with 1 descendant, 2, 3, ..., n-k+2.
            for(int d=1; d <= n-k+1; ++d) {
                long double lnprob = lnconst
                    + lbinom(n-d-1, k-1) - lbinom(n,d);

                // probability of site pattern
                cd.contribution = (double) expl(lnprob);

                // times expected length of interval
                cd.contribution *= elen[k-1];
                
                status = traverseComb(n, d, visitComb, &cd);
                if(status)
                    return status;
            }
        }
    }
    return status;
}

/// Visit a combination
int visitComb(int d, int ndx[d], void *data) {
    assert(d>0);
    CombDat *dat = (CombDat *) data;

    for(IdSet ids = dat->ids; ids != NULL; ids = ids->next) {
        
        // tid is the union of descendants in current set.
        tipId_t tid = 0;
        for(int i=0; i < d; ++i)
            tid |= ids->tid[ndx[i]];

        // Skip singletons unless data->dosing is nonzero
        if(!dat->dosing && isPow2(tid[i]))
            continue;
      
        // Increment BranchTab entry for current tid value.
        BranchTab_add(dat->branchtab, tid,
                      ids->p * dat->contribution);
    }
    return 0;
}
#endif
