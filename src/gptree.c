/**
 * @file gptree.c
 * @brief Methods for simulating gene genealogies within a given tree
 * of populations, and allowing populations to mix and also to split.
 */

#include "gptree.h"
#include "gene.h"
#include "exopar.h"
#include "lblndx.h"
#include "parse.h"
#include "parstore.h"
#include <string.h>
#include <gsl/gsl_rng.h>

#include <pthread.h>
extern pthread_mutex_t outputLock;

struct GPTree {
    int nseg; // number of segments in population tree.
    PopNode *pnv; // array of length nseg
    PopNode *rootPop;
    Gene *rootGene;
    Bounds bnd;
    ExoPar *exopar;
    ParStore *parstore;
    LblNdx lblndx;
    SampNdx sndx;
};

void GPTree_printParStore(GPTree *self, FILE *fp) {
	ParStore_print(self->parstore, fp);
}

void GPTree_printParStoreFree(GPTree *self, FILE *fp) {
	ParStore_printFree(self->parstore, fp);
}

void GPTree_randomize(GPTree *self, gsl_rng *rng) {
	PopNode_randomize(self->rootPop, self->bnd, rng);
}

void GPTree_setParams(GPTree *self, int n, double x[n]) {
    assert(n == ParStore_nFree(self->parstore));
    ParStore_setFreeParams(self->parstore, n, x);
}

void GPTree_getParams(GPTree *self, int n, double x[n]) {
    assert(n == ParStore_nFree(self->parstore));
    ParStore_getFreeParams(self->parstore, n, x);
}

/// Return number of free parameters
int GPTree_nFree(const GPTree *self) {
    return ParStore_nFree(self->parstore);
}

void GPTree_simulate(GPTree *self, BranchTab *branchtab, gsl_rng *rng,
                     unsigned long nreps, int doSing) {
    unsigned long rep;
    for(rep = 0; rep < nreps; ++rep) {
        PopNode_clear(self->rootPop); // remove old samples 
        SampNdx_populateTree(&(self->sndx));    // add new samples
        PopNode_gaussian(self->rootPop, self->bnd,
                         self->exopar, rng);

        // coalescent simulation generates gene genealogy within
        // population tree.
        self->rootGene = PopNode_coalesce(self->rootPop, rng);
        assert(self->rootGene);

        // Traverse gene tree, accumulating branch lengths in bins
        // that correspond to site patterns.
        Gene_tabulate(self->rootGene, branchtab, doSing);

        // Free gene genealogy but not population tree.
        Gene_free(self->rootGene);
        self->rootGene = NULL;
    }
}

GPTree *GPTree_new(const char *fname, Bounds bnd) {
    GPTree *self = malloc(sizeof(GPTree));
    CHECKMEM(self);

    memset(self, 0, sizeof(GPTree));
    self->bnd = bnd;
    self->exopar = ExoPar_new();
    self->parstore = ParStore_new();
    LblNdx_init(&self->lblndx);
    SampNdx_init(&self->sndx);
    FILE *fp = fopen(fname, "r");
    if(fp == NULL)
        eprintf("%s:%s:%d: can't open file \"%s\".\n",
                __FILE__, __func__, __LINE__, fname);
    self->nseg = countSegments(fp);
    rewind(fp);
    self->pnv = malloc(self->nseg * sizeof(self->pnv[0]));
    CHECKMEM(self->pnv);

    NodeStore *ns = NodeStore_new(self->nseg, self->pnv);
    CHECKMEM(ns);

    self->rootPop = mktree(fp, &self->sndx, &self->lblndx, self->parstore,
                           self->exopar, &self->bnd, ns);

    fclose(fp);
    NodeStore_free(ns);
    GPTree_sanityCheck(self, __FILE__, __LINE__);
    if(!GPTree_feasible(self)) {
        fprintf(stderr,"%s:%s:%d: file \"%s\" describes an infeasible tree.\n",
                __FILE__,__func__,__LINE__,fname);
        exit(EXIT_FAILURE);
    }
    return self;
}

/// Destructor.
void GPTree_free(GPTree *self) {
    Gene_free(self->rootGene);
    self->rootGene = NULL;
    PopNode_clear(self->rootPop);
    self->rootPop = NULL;
    free(self->pnv);
    ParStore_free(self->parstore);
    ExoPar_free(self->exopar);
    free(self);
}

/// Duplicate a GPTree object
GPTree *GPTree_dup(const GPTree *old) {
    assert(old);
	assert(GPTree_feasible(old));
    if(old->rootGene != NULL)
        eprintf("%s:%s:%d: old->rootGene must be NULL on entry\n",
                __FILE__,__func__,__LINE__);
    if(!PopNode_isClear(old->rootPop))
        eprintf("%s:%s:%d: clear GPTree of samples before call"
                " to GPTree_dup\n",
                __FILE__,__func__,__LINE__);

    GPTree *new   = memdup(old, sizeof(GPTree));
    new->parstore = ParStore_dup(old->parstore);
    new->exopar = ExoPar_dup(old->exopar);
    new->pnv      = memdup(old->pnv, old->nseg * sizeof(PopNode));

    assert(old->nseg == new->nseg);
    CHECKMEM(new->parstore);
    CHECKMEM(new->exopar);
    CHECKMEM(new->pnv);

    new->sndx = old->sndx;

    /*
     * Adjust the pointers so they refer to the memory allocated in
     * "new" rather than that in "old".  dpar is the absolute offset
     * between new and old for parameter pointers.  dpop is the
     * analogous offset for PopNode pointers. spar and spop are the
     * signs of these offsets. Everything has to be cast to size_t,
     * because we are not doing pointer arithmetic in the usual
     * sense. Ordinarily, ptr+3 means ptr + 3*sizeof(*ptr). We want it
     * to mean ptr+3*sizeof(char).
     */
    int i, spar, spop;
    size_t dpar, dpop;

    // Calculate offsets and signs
    if(new->parstore > old->parstore) {
        dpar = ((size_t) new->parstore) - ((size_t) old->parstore);
        spar = 1;
    }else{
        dpar = ((size_t) old->parstore) - ((size_t) new->parstore);
        spar = -1;
    }
    if(new->pnv > old->pnv) {
        dpop = ((size_t) new->pnv) - ((size_t) old->pnv);
        spop = 1;
    }else{
        dpop = ((size_t) old->pnv) - ((size_t) new->pnv);
        spop = -1;
    }
    
    SHIFT_PTR(new->rootPop, dpop, spop);
    ExoPar_shiftPtrs(new->exopar, dpar, spar);
    for(i=0; i < old->nseg; ++i) {
        PopNode_shiftParamPtrs(&new->pnv[i], dpar, spar);
        PopNode_shiftPopNodePtrs(&new->pnv[i], dpop, spop);
    }
    SampNdx_shiftPtrs(&new->sndx, dpop, spop);
    assert(SampNdx_ptrsLegal(&new->sndx, new->pnv, new->pnv + new->nseg));

    GPTree_sanityCheck(new, __FILE__, __LINE__);
    assert(GPTree_equals(old, new));
    if(!GPTree_feasible(new)) {
        pthread_mutex_lock(&outputLock);
        fflush(stdout);
        dostacktrace(__FILE__,__LINE__,stderr);
        fprintf(stderr,"%s:%d: new tree isn't feasible\n",__FILE__,__LINE__);
        pthread_mutex_unlock(&outputLock);
        exit(EXIT_FAILURE);
    }
	assert(GPTree_feasible(new));
    return new;
}

void GPTree_sanityCheck(GPTree *self, const char *file, int line) {
#ifndef NDEBUG
    REQUIRE(self->nseg > 0,                         file, line);
    REQUIRE(self->pnv != NULL,                      file, line);
    REQUIRE(self->rootPop >= self->pnv,             file, line);
    REQUIRE(self->rootPop < self->pnv + self->nseg, file, line);
    Bounds_sanityCheck(&self->bnd,                  file, line);
    ParStore_sanityCheck(self->parstore,            file, line);
    LblNdx_sanityCheck(&self->lblndx,               file, line);
#endif
}

/// Return 1 if two GPTree objects are equal, 0 if they differ.  Abort
/// with an error if the GPTree pointers are different but one or more
/// of the internal pointers is the same.  Does not access rootPop or
/// rootGene.
int GPTree_equals(const GPTree *lhs, const GPTree *rhs) {
    if(lhs == rhs)
        return 0;
    if(lhs->pnv == rhs->pnv)
        eprintf("%s:%s:%d: two GPTree objects share a PopNode pointer\n",
                __FILE__,__func__,__LINE__);
    if(lhs->parstore == rhs->parstore)
        eprintf("%s:%s:%d: two GPTree objects share a ParStore pointer\n",
                __FILE__,__func__,__LINE__);
    if(!Bounds_equals(&lhs->bnd, &rhs->bnd))
        return 0;
    if(!ParStore_equals(lhs->parstore, rhs->parstore))
        return 0;
    if(!LblNdx_equals(&lhs->lblndx, &rhs->lblndx))
        return 0;
    if(!SampNdx_equals(&lhs->sndx, &rhs->sndx))
        return 0;
    return 1;
}

LblNdx GPTree_getLblNdx(GPTree *self) {
    return self->lblndx;
}

/// Return pointer to array of lower bounds of free parameters
double     *GPTree_loBounds(GPTree *self) {
    return ParStore_loBounds(self->parstore);
}

/// Return pointer to array of upper bounds of free parameters
double     *GPTree_upBounds(GPTree *self) {
    return ParStore_upBounds(self->parstore);
}

/// Return number of samples.
unsigned    GPTree_nsamples(GPTree *self) {
    return SampNdx_size(&self->sndx);
}

/// Are parameters within the feasible region?
int GPTree_feasible(const GPTree *self) {
	return PopNode_feasible(self->rootPop, self->bnd);
}


#ifdef TEST

#include <string.h>
#include <assert.h>

#ifdef NDEBUG
#error "Unit tests must be compiled without -DNDEBUG flag"
#endif

#include <assert.h>
#include <unistd.h>

#ifdef NDEBUG
#error "Unit tests must be compiled without -DNDEBUG flag"
#endif

//      a-------|
//              |ab--|
//      b--|bb--|    |
//         |         |abc--
//         |c--------|
//
//  t = 0  1    3    5.5     inf
const char *tstInput =
    " # this is a comment\n"
    "time fixed  T0=0\n"
    "time free   Tc=1\n"
    "time free   Tab=3\n"
    "time free   Tabc=5.5\n"
    "twoN free   2Na=100\n"
    "twoN fixed  2Nb=123\n"
    "twoN free   2Nc=213.4\n"
    "twoN fixed  2Nbb=32.1\n"
    "twoN free   2Nab=222\n"
    "twoN fixed  2Nabc=1.2e2\n"
    "mixFrac free Mc=0.02\n"
    "segment a   t=T0     twoN=2Na    samples=1\n"
    "segment b   t=T0     twoN=2Nb    samples=1\n"
    "segment c   t=Tc     twoN=2Nc    samples=1\n"
    "segment bb  t=Tc     twoN=2Nbb\n"
    "segment ab  t=Tab    twoN=2Nab\n"
    "segment abc t=Tabc   twoN=2Nabc\n"
    "mix    b  from bb + Mc * c\n"
    "derive a  from ab\n"
    "derive bb from ab\n"
    "derive ab from abc\n"
    "derive c  from abc\n";
int main(int argc, char **argv) {

    int verbose=0;

    if(argc > 1) {
        if(argc!=2 || 0!=strcmp(argv[1], "-v")) {
            fprintf(stderr,"usage: xgptree [-v]\n");
            exit(EXIT_FAILURE);
        }
        verbose = 1;
    }

    const char *fname = "mktree-tmp.lgo";
    FILE       *fp = fopen(fname, "w");
    fputs(tstInput, fp);
    fclose(fp);

	Bounds   bnd = {
		.lo_twoN = 0.0,
		.hi_twoN = 1e7,
		.lo_t = 0.0,
		.hi_t = HUGE_VAL
	};
    GPTree *g = GPTree_new(fname, bnd);
    GPTree *g2 = GPTree_dup(g);
    assert(GPTree_equals(g, g2));

    const LblNdx lblndx = GPTree_getLblNdx(g);
    if(verbose)
        LblNdx_print(&lblndx, stdout);

    GPTree_free(g);
    GPTree_free(g2);

    unlink(fname);
    unitTstResult("GPTree", "untested");
    return 0;
}
#endif
