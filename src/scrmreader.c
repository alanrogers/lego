/**
@file scrmreader.c
@page scrmreader
@brief Interface to scrm output files.
*/

#include "scrmreader.h"
#include "misc.h"
#include "tokenizer.h"
#include "error.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

struct ScrmReader {
    int npops;
    unsigned *nsamples;
    double *daf;

    // Independent replicates in scrm output appear as separate
    // chromosomes, which are labelled by unsigned integers.
    unsigned chr;

    // Position values in scrm output are ignored. Instead, ScrmReader
    // returns positions as a sequence of unsigned integers.
    unsigned long nucpos;
    Tokenizer *tkz;
    FILE *fp;
};

unsigned *countSamples(Tokenizer *tkz, int *npops);
int readuntil(int n, const char *str, int dim, char buff[dim], FILE *fp);

// destructor
void ScrmReader_free(ScrmReader *self) {
    assert(self);
    free(self->nsamples);
    free(self->daf);
    Tokenizer_free(self->tkz);
    free(self);
}

// On input, tkz should point to a tokenized string representing the
// scrm command line, and nsamples should point to an int.
//
// The function returns a newly-allocated array of ints, whose dimension
// is *npops, the number of populations specified on the scrm command line.
// The i'th entry in this array is the haploid sample size of population i.
//
// On error, the function returns NULL.
unsigned *countSamples(Tokenizer *tkz, int *npops) {

    if(strcmp("scrm", Tokenizer_token(tkz, 0)) != 0) {
        fprintf(stderr,"%s:%d: input file is not scrm output\n",
                __FILE__,__LINE__);
        return NULL;
    }

    int i, j;
    long unsigned h;
    char *token, *end;
    unsigned *nsamples = NULL;
    int ntokens = Tokenizer_ntokens(tkz);

    *npops=0;

    // Read through tokens, looking for -I and -eI. Use these arguments
    // to set npops and nsamples.
    for(i=1; i < ntokens; ++i) {
        token = Tokenizer_token(tkz, i);
        if(strcmp("-I", token) == 0 || strcmp("-eI", token) == 0) {
            if(*npops == 0) {
                // count populations and allocate nsamples
                assert(nsamples == NULL);
                for(j=i+2; j<ntokens; ++j) {
                    token = Tokenizer_token(tkz, j);
                    h = strtoul(token, &end, 10);
                    if(end==token) // token isn't an integer
                        break;
                    else           // token is an integer
                        ++*npops;
                }
                if(*npops == 0) {
                    fprintf(stderr,"%s:%d: npops is zero\n",
                            __FILE__,__LINE__);
                    return NULL;
                }
                nsamples = malloc(*npops * sizeof(nsamples[0]));
                CHECKMEM(nsamples);
                memset(nsamples, 0, *npops * sizeof(nsamples[0]));
            }
            // increment nsamples
            assert(*npops > 0);
            assert(nsamples != NULL);
            for(j=0; j < *npops; ++j) {
                token = Tokenizer_token(tkz, i+2+j);
                h = strtoul(token, &end, 10);
                assert(end != token);
                nsamples[j] += h;
            }
            // advance to last argument of -I or -eI
            i += 1 + *npops;
        }
    }
    // Remove populations with zero samples
    *npops = removeZeroes(*npops, nsamples);
    return nsamples;
}

// Allocate and initialize a new ScrmReader from input stream.
ScrmReader *ScrmReader_new(FILE *fp) {

    // buffer is large, because scrm command lines can be long
    char buff[8192];
    int i, status;

    status = readline(sizeof(buff), buff, fp);
    switch(status) {
    case 0:
        break;
    case EOF:
        fprintf(stderr,"%s:%d: unexpected EOF\n", __FILE__,__LINE__);
        return NULL;
    case BUFFER_OVERFLOW:
        fprintf(stderr,"%s:%d: buffer overflow reading scrm command\n",
                __FILE__,__LINE__);
        return NULL;
    default:
        fprintf(stderr,"%s:%d: unknown error\n", __FILE__,__LINE__);
        return NULL;
    }

    ScrmReader *self = malloc(sizeof(ScrmReader));
    CHECKMEM(self);
    memset(self, 0, sizeof(ScrmReader));
    self->fp = fp;

    self->tkz = Tokenizer_new(sizeof(buff)/2);
    Tokenizer_split(self->tkz, buff, " ");
    Tokenizer_strip(self->tkz, " \n");

    self->nsamples = countSamples(self->tkz, &self->npops);

    unsigned tot = 0;
    for(i=0; i < self->npops; ++i) {
        tot += self->nsamples[i];
    }
    unsigned tot2 = strtoul(Tokenizer_token(self->tkz, 1), NULL, 10);
    if(tot != tot2) {
        fprintf(stderr,"%s:%d: incorrect sample count: %u; should be %u\n",
                __FILE__,__LINE__, tot, tot2);
        free(self->nsamples);
        ScrmReader_free(self);
        return NULL;
    }

    // read to line beginning with "position"
    status = readuntil(strlen("position"), "position", sizeof(buff), buff, fp);
    if(status) {
        free(self->nsamples);
        ScrmReader_free(self);
        return NULL;
    }

    // allocate daf array
    self->daf = malloc(self->npops * sizeof(self->daf[0]));
    CHECKMEM(self->daf);

    // read 1st line of data
    status = ScrmReader_next(self);
    if(status) {
        free(self->nsamples);
        free(self->daf);
        ScrmReader_free(self);
        return NULL;
    }
    self->chr = self->nucpos = 0;
    return self;
}

/// Read lines until we reach one that begins with str.
/// Return 0 on success, EOF on failure.
int readuntil(int n, const char *str, int dim, char buff[dim], FILE *fp) {
    int status;
    do{
        status = readline(dim, buff, fp);
        if(status)
            return status;
    }while(0 != strncmp(buff, str, n));
    return 0;
}

// Rewind input and reset chr and nucpos. Doesn't work
// if input is stdin.
int ScrmReader_rewind(ScrmReader *self) {
    int status;
    char buff[8192];
    assert(self->fp != stdin);
    errno = 0;
    rewind(self->fp);
    if(errno)
        return errno;
   // read to line beginning with "position"
    status = readuntil(strlen("position"), "position", sizeof(buff), buff,
                       self->fp);
    if(status) 
        return status;

    status = ScrmReader_next(self);
    if(status) 
        return status;
    self->chr = self->nucpos = 0;
    return 0;
}

// Move ScrmReader to next nucleotide site.
int ScrmReader_next(ScrmReader *self) {
    char buff[8192];
    int status, ntokens;
    status = readline(sizeof(buff), buff, self->fp);
    if(status)
        return status;
    if(strlen(stripWhiteSpace(buff)) == 0) {
        // new chromosome
        status = readuntil(strlen("position"), "position", sizeof(buff), buff,
                           self->fp);
        if(status) 
            return status;

        status = readline(sizeof(buff), buff, self->fp);
        if(status)
            return status;
        ++self->chr;
        self->nucpos = 0;
    }else
        ++self->nucpos;
    Tokenizer_split(self->tkz, buff, " ");
    ntokens = Tokenizer_strip(self->tkz, " \n");

    // calculate derived allele frequency w/i each pop
    double nderived;
    int pop, i;
    int start=2;   // skip 1st two columns
    char *token, *end;
    for(pop=0; pop < self->npops; ++pop) {
        nderived = 0.0;
        for(i=start; i < start + self->nsamples[pop]; ++i) {
            if(i >= Tokenizer_ntokens(self->tkz)) {
                fprintf(stderr,"%s:%s:%d: too few genotypes in scrm output\n",
                        __FILE__,__func__,__LINE__);
                return EDOM;
            }
            token = Tokenizer_token(self->tkz, i);
            unsigned gtype = strtoul(token, &end, 10);
            if(token==end || (gtype!=0 && gtype!=1)) {
                fprintf(stderr,"%s:%d: illegal genotype: %s\n",
                        __FILE__,__LINE__, token);
                return EDOM;
            }
            nderived += gtype;
        }
        self->daf[pop] = nderived / self->nsamples[pop];
        start += self->nsamples[pop];
    }
    return 0;
}

// Return current chromosome.
unsigned ScrmReader_chr(ScrmReader *self) {
    return self->chr;
}

// Return current nucleotide position.
unsigned long ScrmReader_nucpos(ScrmReader *self) {
    return self->nucpos;
}

// Return number of populations.
int ScrmReader_npops(ScrmReader *self) {
    return self->npops;
}

// Return number of samples from population i.
int ScrmReader_nsamples(ScrmReader *self, int i) {
    assert(i < self->npops);
    return self->nsamples[i];
}

// Return frequency of derived allele in sample from population i.
double ScrmReader_daf(ScrmReader *self, int i) {
    assert(i < self->npops);
    return self->daf[i];
}

#ifdef TEST

#  ifdef NDEBUG
#    error "Unit tests must be compiled without -DNDEBUG flag"
#  endif

const char *cmd = "scrm 18 2 -l 100r -t 1.35351 -r 0.966782 1000"
    " -transpose-segsites -SC abs -I 5 6 6 0 0 0 -eI 0.0192475 0 0 2 0 0"
    " -eI 0.00561032 0 0 2 0 0 -eI 0.0117678 0 0 0 2 0 -n 1 2.0687"
    " -n 2 1.12947 -n 3 0.239208 -n 4 0.239208 -n 5 0.239208"
    " -en 0.0391811 1 1.04935 -en 0.268103 1 1 -en 0.238251 3 0.143789"
    " -eg 0 2 0.01 -en 0.00413739 2 0.020687 -en 0.00930914 2 2.0687"
    " -en 0.0174805 2 0.20687 -en 0.0178322 2 1.04935 -eg 0 1 0.005"
    " -en 0.00413739 1 0.20687 -en 0.00930914 1 1.04935 -ej 0.771206 5 1"
    " -ej 0.268103 3 1 -ej 0.0391811 2 1 -ej 0.238251 4 1"
    " -eps 0.0196216 2 3 0.987756 -eps 0.2379 4 5 0.969232";

int main(int argc, char **argv) {
    int         verbose = 0;
    if(argc > 1) {
        if(argc != 2 || 0 != strcmp(argv[1], "-v")) {
            fprintf(stderr, "usage: xscrmreader [-v]\n");
            exit(EXIT_FAILURE);
        }
        verbose = 1;
    }
    int i, npops=0; 
    char buff[1000];
    unsigned *nsamples;
    Tokenizer *tkz = Tokenizer_new(sizeof(buff)/2);
    CHECKMEM(tkz);

    strcpy(buff, cmd);
    Tokenizer_split(tkz, buff, " ");
    Tokenizer_strip(tkz, " \n");
    nsamples = countSamples(tkz, &npops);
    if(verbose) {
        printf("countSamples returned: npops=%d; nsamples =", npops);
        for(i=0; i < npops; ++i)
            printf(" %u", nsamples[i]);
        putchar('\n');
    }

    unitTstResult("countSamples", "OK");

    FILE *fp = fopen("output.scrm", "r");
    int status;
    assert(fp);
    status = readuntil(strlen("position"), "position", sizeof(buff), buff, fp);
    if(status) {
        fprintf(stderr,"%s:%d: readuntil returned %d\n",
                __FILE__,__LINE__, status);
        exit(EXIT_FAILURE);
    }
    if(verbose) {
        fprintf(stderr,"%s:%d: readuntil succeeded: %s\n",
                __FILE__,__LINE__, buff);
    }
    unitTstResult("readuntil", "OK");

    rewind(fp);
    ScrmReader *r = ScrmReader_new(fp);
    assert(0 == ScrmReader_chr(r));
    assert(0 == ScrmReader_nucpos(r));
    int np = ScrmReader_npops(r);
    assert(np == 4);
    assert(6 == ScrmReader_nsamples(r, 0));
    assert(6 == ScrmReader_nsamples(r, 1));
    assert(4 == ScrmReader_nsamples(r, 2));
    assert(2 == ScrmReader_nsamples(r, 3));
    // 0 0 0 0 0 0 | 0 0 0 0 0 0 | 0 0 0 0 | 1 1
    assert(0.0 == ScrmReader_daf(r, 0));
    assert(0.0 == ScrmReader_daf(r, 1));
    assert(0.0 == ScrmReader_daf(r, 2));
    assert(1.0 == ScrmReader_daf(r, 3));
    if(verbose) {
        for(i=0; i<np; ++i)
            printf("daf[%d]=%g\n", i, ScrmReader_daf(r, i));
    }

    status = ScrmReader_next(r);
    if(status) {
        fprintf(stderr,"%s:%d: ERR ScrmReader_next returned %d\n",
                __FILE__,__LINE__, status);
        exit(EXIT_FAILURE);
    }
    assert(0 == ScrmReader_chr(r));
    assert(1 == ScrmReader_nucpos(r));
    // 1 1 1 1 1 1 | 1 1 1 1 0 1 | 1 1 1 1 | 1 1
    assert(1.0 == ScrmReader_daf(r, 0));
    assert(5.0/6.0 == ScrmReader_daf(r, 1));
    assert(1.0 == ScrmReader_daf(r, 2));
    assert(1.0 == ScrmReader_daf(r, 3));

    status = ScrmReader_next(r);
    if(status) {
        fprintf(stderr,"%s:%d: ERR ScrmReader_next returned %d\n",
                __FILE__,__LINE__, status);
        exit(EXIT_FAILURE);
    }
    assert(1 == ScrmReader_chr(r));
    assert(0 == ScrmReader_nucpos(r));
    // 1 0 0 1 1 0 | 1 1 1 1 1 0 | 0 0 0 0 | 0 0
    assert(3.0/6.0 == ScrmReader_daf(r, 0));
    assert(5.0/6.0 == ScrmReader_daf(r, 1));
    assert(0.0 == ScrmReader_daf(r, 2));
    assert(0.0 == ScrmReader_daf(r, 3));
        
    status = ScrmReader_next(r);
    if(status) {
        fprintf(stderr,"%s:%d: ERR ScrmReader_next returned %d\n",
                __FILE__,__LINE__, status);
        exit(EXIT_FAILURE);
    }
    assert(1 == ScrmReader_chr(r));
    assert(1 == ScrmReader_nucpos(r));
    // 0 0 0 0 0 1 | 0 0 0 0 0 0 | 0 0 0 0 | 0 0
    assert(1.0/6.0 == ScrmReader_daf(r, 0));
    assert(0.0 == ScrmReader_daf(r, 1));
    assert(0.0 == ScrmReader_daf(r, 2));
    assert(0.0 == ScrmReader_daf(r, 3));

    status = ScrmReader_rewind(r);
    if(status) {
        fprintf(stderr,"%s:%d: ScrmReader_rewind returned %d\n",
                __FILE__,__LINE__,status);
    }

    assert(0 == ScrmReader_chr(r));
    assert(0 == ScrmReader_nucpos(r));
    np = ScrmReader_npops(r);
    assert(np == 4);
    assert(6 == ScrmReader_nsamples(r, 0));
    assert(6 == ScrmReader_nsamples(r, 1));
    assert(4 == ScrmReader_nsamples(r, 2));
    assert(2 == ScrmReader_nsamples(r, 3));
    // 0 0 0 0 0 0 | 0 0 0 0 0 0 | 0 0 0 0 | 1 1
    assert(0.0 == ScrmReader_daf(r, 0));
    assert(0.0 == ScrmReader_daf(r, 1));
    assert(0.0 == ScrmReader_daf(r, 2));
    assert(1.0 == ScrmReader_daf(r, 3));
    if(verbose) {
        for(i=0; i<np; ++i)
            printf("daf[%d]=%g\n", i, ScrmReader_daf(r, i));
    }

    ScrmReader_free(r);
    unitTstResult("ScrmReader", "OK");
    
    return 0;
}

#endif