#ifndef ARR_POPNODE_H
#  define ARR_POPNODE_H

#  include "typedefs.h"
#  include <stdbool.h>
#  include <gsl/gsl_rng.h>

#  define POPNAMESIZE 30
#  define MAXSAMP ((int)(8*sizeof(tipId_t)))

// Add increment INC to pointer PTR, if PTR!=NULL. Units are
// sizeof(char) rather than the size of the object to which PTR
// refers. 
#define INCR_PTR(PTR,INC) do{                                   \
	if((PTR) != NULL) {											\
        (PTR) = (void *) (((size_t) (PTR)) + ((size_t) (INC))); \
	}															\
}while(0);

struct SampNdx {
    // Array "node" contains an entry for each sample. That entry
    // is a pointer to the node into which the sample should
    // be placed. The sample gets a label of type tipIt_t. For sample
    // i, the label equals 2^i (i.e. 1<<i). There is another class,
    // called LblNdx, which maintains an array of labels. In that
    // array, the i'th label refers to the i'th sample in SampNdx.
    // I keep them separate, because LblNdx needs to be passed to
    // functions that have no need to know about pointers to PopNode
    // objects.
    unsigned    n;              // number of samples
    PopNode    *node[MAXSAMP];
};

struct PopNode {
    int         nparents, nchildren, nsamples;
    double      *twoN;           // ptr to current pop size
    double      *start, *end;    // duration of this PopNode
    double      *mix;            // ptr to frac of pop derived from parent[1]
    struct PopNode *parent[2];
    struct PopNode *child[2];
    Gene       *sample[MAXSAMP];
    bool        twoNfree, startFree, mixFree; // true => parameter varies
    bool        touched;   // true => node has been visited during traversal
};

PopNode    *PopNode_new(double *twoN, bool twoNfree, double *start,
                        bool startFree, NodeStore *ns);
void        PopNode_addChild(PopNode * parent, PopNode * child);
void        PopNode_mix(PopNode * child, double *mPtr, bool mixFree,
                        PopNode * introgressor, PopNode * native);
void        PopNode_newGene(PopNode * self, unsigned ndx);
void        PopNode_addSample(PopNode * self, Gene * gene);
Gene       *PopNode_coalesce(PopNode * self, gsl_rng * rng);
int         PopNode_feasible(const PopNode *self, Bounds bnd);
void        PopNode_free(PopNode * self);
void        PopNode_clear(PopNode * self);
int         PopNode_isClear(const PopNode *self);
void        PopNode_print(FILE * fp, PopNode * self, int indent);
void        PopNode_printShallow(PopNode * self, FILE * fp);
PopNode    *PopNode_root(PopNode * self);
void        PopNode_sanityFromLeaf(PopNode * self, const char *file,
                                   int line);
int         PopNode_nsamples(PopNode * self);
void        PopNode_shiftParamPtrs(PopNode *self, size_t dp);
void        PopNode_shiftPopNodePtrs(PopNode *self, size_t dp);
void        PopNode_untouch(PopNode * self);
void        PopNode_randomize(PopNode *self, Bounds bnd, gsl_rng *rng);

void        SampNdx_init(SampNdx * self);
void        SampNdx_addSamples(SampNdx * self, unsigned nsamples,
							   PopNode * pnode);
void        SampNdx_populateTree(SampNdx * self);
unsigned    SampNdx_size(SampNdx * self);
int         SampNdx_equals(SampNdx *lhs, SampNdx *rhs);
void        SampNdx_sanityCheck(SampNdx *self, const char *file, int line);
int         SampNdx_ptrsLegal(SampNdx *self, PopNode *start, PopNode *end);
void        SampNdx_shiftPtrs(SampNdx *self, size_t dpop);

NodeStore  *NodeStore_new(int len, PopNode *v);
void        NodeStore_free(NodeStore *self);
PopNode    *NodeStore_alloc(NodeStore *self);



#endif
