#ifndef ARR_POINTBUFF_H
#define ARR_POINTBUFF_H

#include "typedefs.h"

PointBuff *PointBuff_new(unsigned npar, unsigned totpts);
void     PointBuff_free(PointBuff * self);
unsigned PointBuff_size(const PointBuff * self);
void     PointBuff_push(PointBuff * self, double cost, int n, double param[n]);
double   PointBuff_pop(PointBuff * self, int n, double param[n]);

#endif
