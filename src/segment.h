#ifndef ARR_SEGMENT_H
#define ARR_SEGMENT_H

#include "typedefs.h"
#include <stdio.h>

// One segment of a population network. This version works
// with MCTree.
struct Segment {
    int             nparents, nchildren, nsamples;
    double         *twoN;        // ptr to current pop size
    double         *start, *end; // duration of this PopNode
    double         *mix;         // ptr to frac of pop derived from parent[1]
    struct Segment *parent[2];
    struct Segment *child[2];

    int max;       // max number of lineages in segment

    // d[i] is a linked list of sets of i+1 descendants
    IdSet         *d[max];

    // a[i] is a linked list of sets of i+1 ancestors
    IdSet         *a[max];

    // p[0][i] is prob there are i+1 lineages at recent end of segment
    // p[1][i] is analogous prob for ancient end of interval.
    double p[2][MAXSAMP];
};

void    *Segment_new(double *twoN, double *start, NodeStore *ns);
int      Segment_coalesce(Segment *self, int maxsamp, int dosing,
                          BranchTab *branchtab);
int      Segment_addChild(void * vparent, void * vchild);
int      Segment_mix(void * vchild, double *mPtr, void * vintrogressor, 
                      void * vnative);
void    *Segment_root(void * vself);
void     Segment_print(FILE * fp, void * self, int indent);
void     Segment_sanityCheck(Segment * self, const char *file, int lineno);

#endif
