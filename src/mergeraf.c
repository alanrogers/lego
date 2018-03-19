/**
@file mergeraf.c
@page mergeraf
@brief Merge two or more raf files

# Mergeraf: merge two or more raf files

Sitepat reads several files in .raf format and prints a single raf file
to standard output. The output includes only those positions at which
chromosome, position, ref, and alt match in all the input
files. (Missing values in alt are allowed.) In the output file, the
reference allele frequency (raf) is the unweighted average of those in
the input files.

# Usage

@copyright Copyright (c) 2018, Alan R. Rogers
<rogers@anthro.utah.edu>. This file is released under the Internet
Systems Consortium License, which can be found in file "LICENSE".
*/

#include "rafreader.h"
#include "error.h"
#include "typedefs.h"
#include "version.h"
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <gsl/gsl_rng.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAXCHR 24               // maximum number of chromosomes

static void usage(void);

const char *useMsg =
    "\nUsage: mergeraf <in_1> <in_2> ...\n"
    "   where <in_i> are input files in raf format."
    "   Writes to standard output.\n";

/// Print usage message and die.
static void usage(void) {
    fputs(useMsg, stderr);
    exit(1);
}

int main(int argc, char **argv) {
    int         i, status, done;
    char        errbuff[100] = { '\0' };

    // Each command line argument is an input file name
    int         n = argc - 1;  // number of input files
    if(n == 0)
        usage();

    RAFReader  *r[n];

    // Each argument should be the name of an input file.
    for(i = 0; i < n; ++i)
        r[i] = RAFReader_new(argv[i+1]);

    printf("# mergeraf version %s\n", VERSION);

    // Iterate through raf files
    printf("#%s\t%s\t%s\t%s\t%s\n", "chr", "pos", "ref", "alt", "raf");
    RAFReader_clearChromosomes(n, r);
    done=0;
    while( !done ) {
        status = RAFReader_multiNext(n, r);
        switch(status) {
        case 0:
            break;
        case EOF:
            done=1;
            continue;
        case REF_MISMATCH:
        case MULTIPLE_ALT:
        case NO_ANCESTRAL_ALLELE:
            continue;
        default:
            // something wrong.
            mystrerror_r(status, errbuff, sizeof errbuff);
            fprintf(stderr,"%s:%d: input error (%s)\n",
                    __FILE__,__LINE__, errbuff);
            exit(EXIT_FAILURE);
        }

        // p is the average frequency of the reference allele
        double p=0.0;
        for(i = 0; i < n; ++i)
            p += RAFReader_raf(r[i]);
        p /= n;

        printf("%s\t%lu\t%s\t%s\t%0.18g\n",
               RAFReader_chr(r[0]),
               RAFReader_nucpos(r[0]),
               RAFReader_ref(r[0]),
               RAFReader_alt(r[0]),
               p);
    }

    for(i = 0; i < n; ++i)
        RAFReader_free(r[i]);
    return 0;
}
