/**
 * @file xsetpart.c
 * @author Alan R. Rogers
 * @brief Generate all partitions of a set of n elements into m subsets.
 * @copyright Copyright (c) 2019, Alan R. Rogers
 * <rogers@anthro.utah.edu>. This file is released under the Internet
 * Systems Consortium License, which can be found in file "LICENSE".
 */

#include "setpart.h"
#include "u64u64map.h"
#include "misc.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#ifdef NDEBUG
#error "Unit tests must be compiled without -DNDEBUG flag"
#endif

void usage(void);
int visit(unsigned n, unsigned a[n], void *data);
int cmpUnsigned(const void *void_x, const void *void_y);

typedef struct VisitDat VisitDat;

// Data manipulated by visit function.
struct VisitDat {
    int verbose, intpart;
    unsigned nparts;
    unsigned long count;
    double lnconst; // log of constant in Durrett's theorem 1.5
    double sumprob; // sum of probabilities should equal 1.
};

/**
 * Compare two unsigned ints.
 *
 * Function interprets its arguments as pointers to ints.
 *
 * @param void_x,void_y Pointers to the two unsigned integers, cast as
 * pointers to voids.
 * @returns -1, 0, or 1 depending on whether the first arg is <,
 * ==, or > the second.
 */
int cmpUnsigned(const void *void_x, const void *void_y) {
    const unsigned *x = (const unsigned *) void_x;
    const unsigned *y = (const unsigned *) void_y;

    return (*x < *y) ? 1 : (*x > *y) ? -1 : 0;
}

/// Function to process partition. This function prints
/// the partition if verbose!=0, checks that it is valid,
/// and counts the partitions. Returns non-zero if an error is found.
int visit(unsigned n, unsigned a[n], void *data) {
    VisitDat *vdat = (VisitDat *) data;

    unsigned max = 0;
    int status=0;
    for(int i=0; i<n; ++i) {
        if(a[i] > max) {
            if(a[i] != max+1)
                ++status;
            max = a[i];
        }
    }
    if(max != 1 + vdat->nparts)
        ++status;
    vdat->count += 1;

    // Calculate the number, k, of partitions and the size, c[i], of each.
    unsigned k = 0;
    for(int i=0; i<n; ++i) {
        if(a[i] > k)
            k = a[i];
    }
    ++k;
    unsigned c[k];
    memset(c, 0, k*sizeof(c[0]));
    for(int i=0; i<n; ++i)
        ++c[a[i]];

    double prob = probPartition(k, c, vdat->lnconst);
    vdat->sumprob += prob;
    if(vdat->intpart) {
        qsort(c, k, sizeof(c[0]), cmpUnsigned);
        for(int i=0; i<k; ++i){
            printf("%u", c[i]);
            if(i < k-1)
                putchar(' ');
        }
        putchar('\n');
    }else if(vdat->verbose) {
        for(int i=0; i<n; ++i) {
            printf("%d", a[i]);
            if(i < n-1)
                putchar(' ');
        }
        printf(" : %le\n", prob);
    }

    return status;
}

void usage(void) {
    fprintf(stderr,"usage: xsetpart [options]\n");
    fprintf(stderr,"where options may include\n");
    fprintf(stderr," -n <n_elements> : size of set\n");
    fprintf(stderr," -k <n_parts>    : number of parts\n");
    fprintf(stderr," -i              : print integer partitions\n");
    fprintf(stderr," -s              : test stirling2 only\n");
    fprintf(stderr," -v              : verbose output\n\n");
    fprintf(stderr,"  n_elements must be > 0\n");
    fprintf(stderr,"  n_parts must be > 0, <= n_elements\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
    int i, n, verbose = 0, s2only=0, intpart=0;
    unsigned nelem = 6, nparts = 3;

    for(i=1; i<argc; ++i) {
        if(strcmp(argv[i], "-v") == 0)
            verbose=1;
        else if(strcmp(argv[i], "-n") == 0) {
            i += 1;
            if(i == argc)
                usage();
            nelem = strtoul(argv[i], NULL, 10);
        }else if(strcmp(argv[i], "-k") == 0) {
            i += 1;
            if(i == argc)
                usage();
            nparts = strtoul(argv[i], NULL, 10);
        }else if(strcmp(argv[i], "-s") == 0) {
            s2only = 1;
        }else if(strcmp(argv[i], "-i") == 0) {
            intpart = 1;
        }else
            usage();
    }

    if(verbose)
        printf("stirling2(%u,%u)=%llu\n", nelem, nparts,
               stirling2(nelem, nparts));

    assert(stirling2(0, 0) == 1ULL);
    for(n=1; n <= nelem; ++n) {
        assert(stirling2(n, 0) == 0);
        assert(stirling2(0, n) == 0);
        assert(stirling2(n, 1) == 1);
        assert(stirling2(n, n) == 1);
    }

    if(nelem >= 3) {
        assert(stirling2(3, 2) == 3);
    }

    if(nelem >= 4) {
        assert(stirling2(4, 2) == 7);
        assert(stirling2(4, 3) == 6);
    }

    if(nelem >= 5) {
        assert(stirling2(5, 2) == 15);
        assert(stirling2(5, 3) == 25);
        assert(stirling2(5, 4) == 10);
    }

    if(nelem >= 6) {
        assert(stirling2(6, 2) == 31);
        assert(stirling2(6, 3) == 90);
        assert(stirling2(6, 4) == 65);
        assert(stirling2(6, 5) == 15);
    }

    unitTstResult("stirling2", "OK");

    if(s2only)
        return 0;

    long double c = lnCoalConst(4, 4);
    unsigned k=4, y[4] = {1u, 1u, 1u, 1u};
    double x = probPartition(k, y, c);
    if(verbose) {
        printf("lnCoalConst(4, 4) = %Lf\n", c);
        printf("probPartition(4, (1,1,1,1) %Lf) = %lf\n", c, x);
    }
    assert(0.0L == c);
    assert(1.0 == x);

    VisitDat vdat = {.verbose = verbose,
                     .intpart = intpart,
                     .nparts = nparts,
                     .count=0,
                     .sumprob=0.0,
                     .lnconst=lnCoalConst(nelem, nparts) };

    if(verbose)
        printf("lnconst=%lf\n", vdat.lnconst);

    int status = traverseSetPartitions(nelem, nparts, visit, &vdat);

    if(verbose)
        printf("sumprob=%lf; should be 1.0; err=%le\n",
               vdat.sumprob, vdat.sumprob - 1.0);
    assert(fabs(vdat.sumprob - 1.0) < 1e-8);

    unitTstResult("probPartition", "OK");
    
    assert(vdat.count == stirling2(nelem, nparts));

    // Free hash table stored by stirling2. This isn't really
    // necessary.  I do it to keep valgrind quiet.
    stirling2_free();

    unitTstResult("SetPart", status==0 ? "OK" : "FAIL");
    return 0;
}
