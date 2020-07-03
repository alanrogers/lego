/**
 * @file xptrptrmap.c
 * @author Alan R. Rogers
 * @brief Test ptrptrmap.c.
 * @copyright Copyright (c) 2020, Alan R. Rogers
 * <rogers@anthro.utah.edu>. This file is released under the Internet
 * Systems Consortium License, which can be found in file "LICENSE".
 */

#include "ptrptrmap.h"
#include "misc.h"
#include "error.h"
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef NDEBUG
#error "Unit tests must be compiled without -DNDEBUG flag"
#endif

int main(int argc, char **argv) {
    int verbose = 0;

    switch (argc) {
    case 1:
        break;
    case 2:
        if(strncmp(argv[1], "-v", 2) != 0) {
            fprintf(stderr, "usage: xptrptrmap [-v]\n");
            exit(EXIT_FAILURE);
        }
        verbose = 1;
        break;
    default:
        fprintf(stderr, "usage: xptrptrmap [-v]\n");
        exit(EXIT_FAILURE);
    }


    PtrPtrMap *map = PtrPtrMap_new();

    const int nvals = 50;
    unsigned key[nvals], value[nvals];
    void *val;
    int status;

    for(int i=0; i < nvals; ++i) {
        key[i] = rand();
        value[i] = rand();
        status = PtrPtrMap_insert(map, key+i, value+i);
        assert(status == 0);
    }

    unsigned long size = PtrPtrMap_size(map);
    assert(nvals == size);

    void *kk[size];
    status = PtrPtrMap_keys(map, size, kk);
    assert(status == 0);

    if(verbose) {
        for(int i=0; i<size; ++i) {
            val = PtrPtrMap_get(map, kk[i], &status);
            assert(status==0);
            printf("%p => %p\n", kk[i], val);
        }                 
    }

    status = PtrPtrMap_keys(map, size/2, kk);
    assert(status == BUFFER_OVERFLOW);

    for(int i=0; i < nvals; ++i) {
        val = PtrPtrMap_get(map, key+i, &status);
        assert(status==0);
        assert(*((unsigned *)val) == value[i]);
    }

    // k is not a key in the map
    unsigned *k = key + nvals;

    val = PtrPtrMap_get(map, k, &status);
    assert(status == 1);

    PtrPtrMap_free(map);

    unitTstResult("PtrPtrMap", "OK");

    return 0;
}
