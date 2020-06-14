#ifndef ARR_PTRVEC_H
#define ARR_PTRVEC_H

#include "typedefs.h"
#include <assert.h>

struct PtrVec {
    unsigned buffsize;
    unsigned used;
    void **buff;  // array of pointers
};

PtrVec *PtrVec_new(unsigned n);
void    PtrVec_free(PtrVec *self);
int     PtrVec_push(PtrVec *self, void *val);
void   *PtrVec_pop(PtrVec *self);
void    PtrVec_freeHoldings(PtrVec *self);
static inline void *PtrVec_get(PtrVec *self, unsigned i);

static inline void *PtrVec_get(PtrVec *self, unsigned i) {
    assert(i < self->used);
    return self->buff[i];
}
#endif
