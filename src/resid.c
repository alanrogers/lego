/**
@file resid.c
@page resid
@author Alan R. Rogers
@brief Print site pattern frequencies or residuals.

# `resid`: print site pattern frequencies or residuals

This program reads a list of data files (containing site pattern
frequencies) and (optionally) another list of .legofit files
(containing estimates as generated by `legofit`). The .legofit files,
if any, should be listed in the same order as the data files, so that
the i'th .legofit file describes the output generated from the i'th
data file. If there are no .legofit files, `resid` prints a table of
site pattern frequencies, in which the i'th row refers to the i'th
site pattern, and the j'th column to the j'th data set. If .legofit
files are provided, the program prints residuals, calculated by
subtracting fitted frequencies from observed ones.  If .legofit files
are provided, columns are labeled with their names, after stripping
suffixes and leading pathnames. Otherwise, column labels are based on
the names of data files.

Optionally, the used can specify a colon-separated list of population 
labels to delete from the output:

   resid data.opf -D x:y

In addition, the user can specify one or more remappings, each of which
tells `resid` to collapse several populations into a single population.
For example,

    resid data.opf -L data.legofit -M n=a:v r=n:x

would calculate residuals assuming that data.opf contains observed
pattern frequencies, and data.legofit contains fitted values, as
produced by legofit. Before producing output, it would apply two
remappings. First, populations "a" and "v" would be collapsed into a
single population called "n". Then, "n" and "x" would be pooled into
"r". The labels "n" and "r" must not be present in the original data.

Note that the second remapping remaps "n", which was defined in the
first. This is legal, but it would not work if the two remappings had
been listed in opposite order. The labels listed on the right side of
a remapping must be defined previously, either in the original data or
in a previous remapping.

Deletions are applied before remappings, so it is illegal to remap
labels that have been deleted using -D.

It is also possible to list multiple sets of data and .legofit
files. For example,

    resid data.opf boot/boot*.opf -L b2.legofit b2boot*.legofit

This would generate a column for each pair of .opf/.legofit files.

@copyright Copyright (c) 2019, Alan R. Rogers
<rogers@anthro.utah.edu>. This file is released under the Internet
Systems Consortium License, which can be found in file "LICENSE".
*/

#include "error.h"
#include "strdblqueue.h"
#include "misc.h"
#include "branchtab.h"
#include <strings.h>
#include <time.h>

struct Mapping {
    char *lhs, *rhs;
};

typedef struct Mapping Mapping;

// Function prototypes
void usage(void);
int Mapping_size(Mapping * self);
Mapping *Mapping_new(const char *lhs, const char *rhs);
void Mapping_free(Mapping * self);
void Mapping_push(Mapping * self, const char *str);
const char *Mapping_lhs(Mapping * self);
const char *Mapping_rhs(Mapping * self);
void Mapping_print(Mapping * self, FILE * fp);

Mapping *Mapping_new(const char *lhs, const char *rhs) {
    Mapping *self = malloc(sizeof(Mapping));
    CHECKMEM(self);
    self->lhs = strdup(lhs);
    CHECKMEM(self->lhs);
    self->rhs = strdup(rhs);
    CHECKMEM(self->rhs);
    return self;
}

void Mapping_free(Mapping * self) {
    free(self->rhs);
    free(self->lhs);
    free(self);
}

int Mapping_size(Mapping * self) {
    return 1 + strchrcnt(self->rhs, ':');
}

// Get label of left-hand side
const char *Mapping_lhs(Mapping * self) {
    return self->lhs;
}

// Get label describing populations to be collapsed
const char *Mapping_rhs(Mapping * self) {
    return self->rhs;
}

void Mapping_print(Mapping * self, FILE * fp) {
    fprintf(fp, "# merging %s -> %s\n", self->rhs, self->lhs);
}

//vars
const char *usageMsg =
    "\nusage: resid [options] <d1>  <d2> ... [-L <f1> <f2> ...]"
    " [-D x:y:z] \\\n"
    "   [-M c=a:b d=c:e:f ...]\n\n"
    "where <d1>, <d2>, ... are names of files containing observed\n"
    "site pattern frequencies, and <f1>, <f2>, ... contain the\n"
    "corresponding fitted values as produced by legofit. Must include\n"
    "at least one data file. Fitted files are optional. If present, their\n"
    "number must equal that of the data files. The optional -D argument\n"
    "introduces a colon-separated list of populations to delete. The\n"
    "optional -M argument introduces one or more remappings, which collapse\n"
    "two or more populations into a single label. Data files must precede\n"
    "the -L, -D, and -M arguments on the command line.\n\n"
    "Options:\n" "   -h or --help   : print this message.\n";

void usage(void) {
    fputs(usageMsg, stderr);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {

    time_t currtime = time(NULL);

    hdr("resid: print site pattern frequencies or residuals");
#if defined(__DATE__) && defined(__TIME__)
    printf("# Program was compiled: %s %s\n", __DATE__, __TIME__);
#endif
    printf("# Program was run: %s\n", ctime(&currtime));
    fflush(stdout);

    enum input_state { DATA, LEGO, DELETE, REMAP };

    enum input_state state = DATA;
    int status = 0;
    int i, j;
    int nDataFiles = 0, nLegoFiles = 0, nmapping = 0;
    unsigned nsamples=0, nsamples2=0, nsamples3=0;
    char *deleteStr = NULL;

    for(i = 1; i < argc; i++) {
        if(argv[i][0] == '-') {
            if(strcmp(argv[i], "-L") == 0) {
                state = LEGO;
                continue;
            } else if(strcmp(argv[i], "-D") == 0) {
                state = DELETE;
                continue;
            } else if(strcmp(argv[i], "-M") == 0) {
                state = REMAP;
                continue;
            } else if(strcmp(argv[i], "-h") == 0 ||
                      strcmp(argv[i], "--help") == 0) {
                usage();
            } else {
                fprintf(stderr, "unknown flag argument: %s\n", argv[i]);
                usage();
            }
        }
        switch (state) {
        case DATA:
            ++nDataFiles;
            break;
        case LEGO:
            ++nLegoFiles;
            break;
        case DELETE:
            if(deleteStr != NULL) {
                fprintf(stderr, "Only one delete string is allowed.\n");
                usage();
            }
            deleteStr = strdup(argv[i]);
            break;
        case REMAP:
            ++nmapping;
            break;
        default:
            fprintf(stderr, "%s:%d: unknown state\n", __FILE__, __LINE__);
            exit(EXIT_FAILURE);
        }
    }
    if(nLegoFiles > 0 && (nDataFiles != nLegoFiles)) {
        fprintf(stderr, "%s:%d\n"
                " Num legofit files must equal either 0 or the number\n"
                " of data files. Instead, there are"
                " %d data files and %d legofit files\n",
                __FILE__, __LINE__, nDataFiles, nLegoFiles);
        usage();
    }

    if(nDataFiles < 1) {
        fprintf(stderr, "nDataFiles=%d; need at least 1\n", nDataFiles);
        usage();
    }

    const char **datafname = NULL, **legofname = NULL;
    datafname = malloc(nDataFiles * sizeof(datafname[0]));
    CHECKMEM(datafname);
    if(nLegoFiles > 0) {
        legofname = malloc(nLegoFiles * sizeof(legofname[0]));
        CHECKMEM(legofname);
    }

    Mapping *mapping[nmapping];
    if(nmapping > 0)
        memset(mapping, 0, nmapping * sizeof(*mapping));
    state = DATA;
    int idata, ilego, imapping;
    idata = ilego = imapping = 0;
    for(i = 1; i < argc; i++) {
        if(argv[i][0] == '-') {
            if(strcmp(argv[i], "-L") == 0) {
                state = LEGO;
                continue;
            } else if(strcmp(argv[i], "-D") == 0) {
                state = DELETE;
                continue;
            } else if(strcmp(argv[i], "-M") == 0) {
                state = REMAP;
                continue;
            } else if(strcmp(argv[i], "-h") == 0 ||
                      strcmp(argv[i], "--help") == 0) {
                usage();
            } else {
                fprintf(stderr, "unknown flag argument: %s\n", argv[i]);
                usage();
            }
        }
        switch (state) {
        case DATA:
            datafname[idata++] = argv[i];
            break;
        case DELETE:
            break;
        case LEGO:
            legofname[ilego++] = argv[i];
            break;
        case REMAP:
        {
            // parse string of form a=b:c:d
            char *next = argv[i];
            char *token = strsep(&next, "=");
            if(NULL == strchr(next, ':')) {
                fprintf(stderr, "%s:%d remapping (%s) in wrong format.\n"
                        "Expecting 2 or more labels separated by ':'"
                        " characters.\n", __FILE__, __LINE__, next);
                exit(EXIT_FAILURE);
            }
            mapping[imapping] = Mapping_new(token, next);
            CHECKMEM(mapping[imapping]);
        }
            ++imapping;
            break;
        default:
            fprintf(stderr, "%s:%d: unknown state\n", __FILE__, __LINE__);
            exit(EXIT_FAILURE);
        }
    }
    assert(idata == nDataFiles);
    assert(ilego == nLegoFiles);
    assert(imapping == nmapping);

    if(deleteStr)
        printf("# deleting %s\n", deleteStr);

    for(imapping = 0; imapping < nmapping; ++imapping) {
        Mapping_print(mapping[imapping], stdout);
    }

    // Read data and legofit files into an arrays of queues
    StrDblQueue *data_queue[nDataFiles], *lego_queue[nLegoFiles];
    memset(data_queue, 0, nDataFiles * sizeof(data_queue[0]));
    if(nLegoFiles)
        memset(lego_queue, 0, nLegoFiles * sizeof(lego_queue[0]));

    for(i = 0; i < nDataFiles; ++i) {
        if(nLegoFiles) {
            lego_queue[i] = StrDblQueue_parseSitePat(legofname[i]);
            StrDblQueue_normalize(lego_queue[i]);
        }
        data_queue[i] = StrDblQueue_parseSitePat(datafname[i]);
        StrDblQueue_normalize(data_queue[i]);
        if(i == 0 && nLegoFiles) {
            checkConsistency(datafname[0], legofname[0],
                             data_queue[0], lego_queue[0]);
            continue;
        }
        checkConsistency(datafname[0], datafname[i],
                         data_queue[0], data_queue[i]);
        if(nLegoFiles)
            checkConsistency(legofname[0], legofname[i],
                             lego_queue[0], lego_queue[i]);
    }

    // Create LblNdx object with an entry for each population
    tipId_t tid;
    LblNdx lndx;
    LblNdx_init(&lndx);
    tipId_t collapse[nmapping], collapse2[nmapping], collapse3[nmapping];
    if(nmapping > 0)
        memset(collapse, 0, nmapping * sizeof(collapse[0]));
    StrDblQueue *sdq;
    for(sdq = data_queue[0]; sdq; sdq = sdq->next) {
        char *s = sdq->strdbl.str;
        while(1) {
            char *colon = strchr(s, ':');
            int len;
            if(colon == NULL) {
                tid = LblNdx_getTipId(&lndx, s);
                if(tid == 0)
                    LblNdx_addSamples(&lndx, 1u, s);
                break;
            }
            // The tokens in s are separated by colons. Copy
            // token into a NULL-terminated string that can
            // be passed to LblNdx_addSamples.
            char buff[100];
            len = colon - s;
            status = strnncopy(sizeof(buff), buff, len, s);
            if(status) {
                fprintf(stderr, "%s:%d: buffer overflow\n",
                        __FILE__, __LINE__);
                exit(EXIT_FAILURE);
            }
            // Add label unless it's already there.
            tid = LblNdx_getTipId(&lndx, s);
            if(tid == 0)
                LblNdx_addSamples(&lndx, 1u, buff);
            s = colon + 1;
        }
    }

    nsamples = LblNdx_size(&lndx);
    tipId_t union_all_samples = low_bits_on(nsamples);

    // Now that lndx has been set, create a copy.
    LblNdx lndx2 = lndx;

    // number of site patterns
    int npat = 0;

    // Arrays to be allocated once we have the collapsed dimension.
    // resid[i*nDataFiles + j] is residual for pattern i in file j
    double *mat = NULL;
    tipId_t *pat = NULL;

    // Parse deleteStr to set delete, and then delete pops from
    // lndx2.
    tipId_t delete = 0;
    if(deleteStr != NULL) {
        delete = LblNdx_getTipId(&lndx, deleteStr);
        if(delete == 0) {
            fprintf(stderr, "%s:%d: delete string (%s) includes"
                    " nonexistent populations.\n",
                    __FILE__, __LINE__, deleteStr);
            exit(EXIT_FAILURE);
        }
        LblNdx_rmPops(&lndx2, delete);
    }
    // Apply remappings in sequence to produce a vector of collapse
    // integers and a single LblNdx object, which describes the result
    // of all remappings.
    for(imapping = 0; imapping < nmapping; ++imapping) {
        const char *rhs = Mapping_rhs(mapping[imapping]);
        collapse[imapping] = LblNdx_getTipId(&lndx2, rhs);
        if(collapse[imapping] == 0) {
            fprintf(stderr, "%s:%d: Remapping %d contains undefined label.\n",
                    __FILE__, __LINE__, imapping);
            Mapping_print(mapping[imapping], stderr);
            exit(EXIT_FAILURE);
        }
        status = LblNdx_collapse(&lndx2, collapse[imapping],
                                 Mapping_lhs(mapping[imapping]));
        switch(status) {
        case EDOM:
            fprintf(stderr,"%s:%d: illegal collapse value: o%o\n",
                    __FILE__,__LINE__, collapse[imapping]);
            exit(EXIT_FAILURE);
        case BUFFER_OVERFLOW:
            fprintf(stderr,"%s:%d: label too long: %s\n",
                    __FILE__,__LINE__,
                    Mapping_lhs(mapping[imapping]));
            exit(EXIT_FAILURE);
        default:
            continue;
        }
    }

    // Redefine collapse vector in original terms
    for(imapping = 0; imapping < nmapping; ++imapping) {
        const char *rhs = Mapping_rhs(mapping[imapping]);
        collapse[imapping] = LblNdx_getTipId(&lndx, rhs);
    }

    // Each pass through the loop calculates residuals for a pair
    // of files: a data file and a legofit file.
    for(i = 0; i < nDataFiles; ++i) {
        StrDbl strdbl;

        nsamples2 = nsamples3 = nsamples;

        // Convert queue to BranchTab object
        BranchTab *resid = BranchTab_new(nsamples);
        while(data_queue[i] != NULL) {
            data_queue[i] = StrDblQueue_pop(data_queue[i], &strdbl);
            tid = LblNdx_getTipId(&lndx, strdbl.str);
            if(tid == 0) {
                fprintf(stderr, "%s:%d:unknown label (%s)\n",
                        __FILE__, __LINE__, strdbl.str);
                exit(EXIT_FAILURE);
            }
            BranchTab_add(resid, tid, strdbl.val);
        }
        BranchTab *fitted = BranchTab_new(nsamples);
        while(nLegoFiles && (lego_queue[i] != NULL)) {
            lego_queue[i] = StrDblQueue_pop(lego_queue[i], &strdbl);
            tid = LblNdx_getTipId(&lndx, strdbl.str);
            if(tid == 0) {
                fprintf(stderr, "%s:%d:unknown label (%s)\n",
                        __FILE__, __LINE__, strdbl.str);
                exit(EXIT_FAILURE);
            }
            BranchTab_add(fitted, tid, strdbl.val);
        }

        BranchTab *tmp;
        tipId_t map[nsamples];
        memset(map, 0, nsamples*sizeof(map[0]));

        // deletions
        if(delete == 0) {
            memcpy(collapse2, collapse, nmapping*sizeof(collapse[0]));
        }else{
            nsamples2 = nsamples - num1bits(delete);

            // Make map, an array whose i'th entry is an unsigned integer
            // with one bit on and the rest off. The on bit indicates the
            // position in the new id of the i'th bit in the old id.
            make_rm_map(nsamples, map, delete);
            
            // Create a new residual BranchTab
            tmp = BranchTab_rmPops(resid, nsamples2, map);
            BranchTab_free(resid);
            resid = tmp;
            BranchTab_sanityCheck(resid, __FILE__,__LINE__);

            // New fitted BranchTab
            if(nLegoFiles) {
                tmp = BranchTab_rmPops(fitted, nsamples2, map);
                BranchTab_free(fitted);
                fitted = tmp;
                BranchTab_sanityCheck(fitted, __FILE__,__LINE__);
            }

            fprintf(stderr,"Deletion map\n");
            for(int k=0; k < nsamples; ++k)
                fprintf(stderr,"%s:%d: map[o%o] = o%o\n",
                        __FILE__,__LINE__,(1<<k), map[k]);

            // Re-express collapse values in terms of reduced set
            // of populations.
            for(imapping = 0; imapping < nmapping; ++imapping) {
                collapse2[imapping] = remap_bits(nsamples, map,
                                                 collapse[imapping]);
            }
        }

        // remappings
        if(nmapping == 0) {
            memcpy(collapse3, collapse2, nmapping*sizeof(collapse[0]));
        }else{
            for(imapping = 0; imapping < nmapping; ++imapping) {
                memset(map, 0, nsamples*sizeof(map[0]));

                // nsamples2 is now the dimension of map.
                make_collapse_map(nsamples2, map, collapse2[imapping]);
            
                fprintf(stderr,"Collapse map for o%o\n", collapse2[imapping]);
                for(int k=0; k < nsamples2; ++k)
                    fprintf(stderr,"%s:%d: map[o%o] = o%o\n",
                            __FILE__,__LINE__,(1<<k), map[k]);

                // number of samples after this collapse operation.
                nsamples3 = nsamples2 - num1bits(collapse2[imapping]) + 1;

                // Dimension of new BranchTab is nsamples3
                tmp = BranchTab_collapse(resid, nsamples3, map);
                BranchTab_free(resid);
                resid = tmp;
                BranchTab_sanityCheck(resid, __FILE__,__LINE__);

                if(nLegoFiles) {
                    tmp = BranchTab_collapse(fitted, nsamples3, map);
                    BranchTab_free(fitted);
                    fitted = tmp;
                    BranchTab_sanityCheck(fitted, __FILE__,__LINE__);
                }

                // Re-express collapse values in terms of reduced set
                // of populations.
                for(imapping = 0; imapping < nmapping; ++imapping)
                    collapse3[imapping] = remap_bits(nsamples2, map,
                                                     collapse2[imapping]);

                nsamples2 = nsamples3;
            }
        }

        // Set npat, pat. Allocate mat.
        if(npat == 0) {
            long double *frq = NULL;
            npat = BranchTab_size(resid);
            mat = malloc(npat * nDataFiles * sizeof(*mat));
            pat = malloc(npat * sizeof(*pat));
            frq = malloc(npat * sizeof(*frq));
            CHECKMEM(mat);
            CHECKMEM(pat);
            CHECKMEM(frq);
            memset(mat, 255u, npat * nDataFiles * sizeof(*mat));
            BranchTab_toArrays(resid, npat, pat, frq);
            qsort(pat, (size_t) npat, sizeof(pat[0]), compare_tipId);
            free(frq);
        }

        // Normalize the BranchTabs
        if(BranchTab_normalize(resid))
            DIE("can't normalize empty BranchTab");
        if(nLegoFiles) {
            if(BranchTab_normalize(fitted))
                DIE("can't normalize empty BranchTab");

            // Calculate residuals for current pair of files.
            // This only happens if nLegoFiles > 0. Otherwise
            // "resid" contains site pattern frequencies, not
            // residuals.
            BranchTab_minusEquals(resid, fitted);
        }

        // re-express any existing entries in mat
        for(int k=0; k < i; ++k) {
            for(j=0; j < npat; ++j)
                mat[j * nDataFiles + k] = remap_bits(nsamples2, map,
                                                     collapse2[imapping]);
        }
        
        // store result in matrix
        for(j = 0; j < npat; ++j)
            mat[j * nDataFiles + i] = BranchTab_get(resid, pat[j]);

        BranchTab_free(resid);
        BranchTab_free(fitted);
    }

    if(nLegoFiles)
        printf("# Printing residuals\n");
    else
        printf("# Printing site pattern frequencies\n");

    // output: remove suffix from file names and truncate
    printf("%-10s", "SitePat");
    if(nLegoFiles) {
        for(j = 0; j < nLegoFiles; ++j) {
            char *p = strrchr(legofname[j], '.');
            if(p)
                *p = '\0';
            printf(" %13.13s", mybasename(legofname[j]));
        }
    } else {
        for(j = 0; j < nDataFiles; ++j) {
            char *p = strrchr(datafname[j], '.');
            if(p)
                *p = '\0';
            printf(" %13.13s", mybasename(datafname[j]));
        }
    }
    putchar('\n');

    for(i = 0; i < npat; ++i) {
        if(pat[i] == union_all_samples)
            continue;

        assert(nlz(pat[i]) >= 8*sizeof(tipId_t) - nsamples2);

        char lbl[100];
        patLbl(sizeof(lbl), lbl, pat[i], &lndx2); //err
        printf("%-10s", lbl);
        for(j = 0; j < nDataFiles; ++j)
            printf(" %13.10lf", mat[i * nDataFiles + j]);
        putchar('\n');
    }

    free(datafname);
    if(legofname)
        free(legofname);
    free(mat);
    free(pat);

    return 0;
}
