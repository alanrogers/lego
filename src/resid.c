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
#include "collapse.h"
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
BranchTab *makeBranchTab(StrDblQueue *queue, LblNdx *lndx);

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

/// Construct a BranchTab from a StrDblQueue. The LblNdx translates
/// site patterns from string representation to integer
/// representation.  Function returns a newly-allocated BranchTab.
/// If queue is NULL, function returns NULL. This function empties
/// the queue, which should therefore be set to NULL on return and not
/// freed.
BranchTab *makeBranchTab(StrDblQueue *queue, LblNdx *lndx) {
    if(queue == NULL)
        return NULL;
    unsigned nsamples = LblNdx_size(lndx);
    BranchTab *new = BranchTab_new(nsamples);
    while(queue != NULL) {
        StrDbl strdbl;
        queue = StrDblQueue_pop(queue, &strdbl);
        tipId_t tid = LblNdx_getTipId(lndx, strdbl.str);
        if(tid == 0) {
            fprintf(stderr, "%s:%d: site pattern string (%s)"
                    " contains unknown label.\n",
                    __FILE__, __LINE__, strdbl.str);
            fprintf(stderr, "Known labels (with indices):\n");
            LblNdx_print(lndx, stderr);
            exit(EXIT_FAILURE);
        }
        BranchTab_add(new, tid, strdbl.val);
    }
    return new;
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
            ++imapping;
        }
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
    // and check for consistency
    StrDblQueue *data_queue[nDataFiles], *lego_queue[nDataFiles];
    for(i = 0; i < nDataFiles; ++i) {
        data_queue[i] = StrDblQueue_parseSitePat(datafname[i]);
        StrDblQueue_normalize(data_queue[i]);

        if(nLegoFiles) {
            lego_queue[i] = StrDblQueue_parseSitePat(legofname[i]);
            StrDblQueue_normalize(lego_queue[i]);
            checkConsistency(datafname[i], legofname[i],
                             data_queue[i], lego_queue[i]);
        }else
            lego_queue[i] = NULL;

        if(i > 0)
            checkConsistency(datafname[0], datafname[i],
                             data_queue[0], data_queue[i]);
    }

    // Create LblNdx, which indexes sample labels.
    LblNdx lblndx;
    status = LblNdx_from_StrDblQueue(&lblndx, data_queue[0]);
    if(status) {
        fprintf(stderr,"%s:%d: StrDblQueue has a field that's too long:\n",
                __FILE__,__LINE__);
        StrDblQueue_print(data_queue[0], stderr);
        exit(EXIT_FAILURE);
    }

    unsigned nsamples = LblNdx_size(&lblndx);
    tipId_t union_all_samples = low_bits_on(nsamples);

    // Make a BranchTab for each data_queue and each lego_queue.
    // Free the queues.
    BranchTab *obs_bt[nDataFiles], *lego_bt[nDataFiles];
    for(i = 0; i < nDataFiles; ++i) {
        obs_bt[i] = makeBranchTab(data_queue[i], &lblndx);
        data_queue[i] = NULL;

        if(nLegoFiles) {
            lego_bt[i] = makeBranchTab(lego_queue[i], &lblndx);
            lego_queue[i] = NULL;
        }else
            lego_bt[i] = NULL;
    }
    
    LblNdx lndx; // lndx will have reduced dimension
    for(i = 0; i < nDataFiles; ++i) {
        LblNdxBranchTab lnbt;

        // lndx starts each iteration with full dimension
        lndx = lblndx;
        
        if(deleteStr) {
            lnbt = removePops(lndx, obs_bt[i], deleteStr);
            BranchTab_free(obs_bt[i]);
            obs_bt[i] = lnbt.branchtab;
            lnbt.branchtab = NULL;
            
            if(nLegoFiles) {
                lnbt = removePops(lndx, lego_bt[i], deleteStr);
                BranchTab_free(lego_bt[i]);
                lego_bt[i] = lnbt.branchtab;
                lnbt.branchtab = NULL;
            }
            lndx = lnbt.lndx;
        }

        // Apply remappings in sequence. Each remapping changes lndx
        // and obs_bt[i].
        for(imapping = 0; imapping < nmapping; ++imapping) {
            const char *rhs = Mapping_rhs(mapping[imapping]);
            const char *lhs = Mapping_lhs(mapping[imapping]);
            lnbt = collapsePops(lndx, obs_bt[i], rhs, lhs);

            BranchTab_free(obs_bt[i]);
            obs_bt[i] = lnbt.branchtab;
            lnbt.branchtab = NULL;

            if(nLegoFiles) {
                lnbt = collapsePops(lndx, lego_bt[i], rhs, lhs);
                BranchTab_free(lego_bt[i]);
                lego_bt[i] = lnbt.branchtab;
                lnbt.branchtab = NULL;
            }

            lndx = lnbt.lndx;
        }

        // Normalize the BranchTabs and subtract expected frequencies
        // (as given in lego_bt, if that exists) from observed
        // frequencies (as given in obs_bt).
        if(BranchTab_normalize(obs_bt[i]))
            DIE("can't normalize empty BranchTab");

        if(nLegoFiles) {
            if(BranchTab_normalize(lego_bt[i]))
                DIE("can't normalize empty BranchTab");

            // Calculate residuals for current pair of files.
            BranchTab_minusEquals(obs_bt[i], lego_bt[i]);
        }
    }

    // Set up arrays.
    int npat = BranchTab_size(obs_bt[0]); // number of site patterns
    tipId_t pat[npat];
    long double frq[npat]; // not used

    BranchTab_toArrays(obs_bt[0], npat, pat, frq);
    qsort(pat, (size_t) npat, sizeof(pat[0]), compare_tipId);

    // mat[i*nDataFiles + j] is residual (or frequency) for pattern i
    // in file j.
    double mat[npat * nDataFiles];
    for(j = 0; j < nDataFiles; ++j) {
        for(i=0; i < npat; ++i)
            mat[i*nDataFiles + j] = BranchTab_get(obs_bt[j], pat[i]);
    }

    if(nLegoFiles)
        printf("# Printing residuals\n");
    else
        printf("# Printing relative frequencies\n");

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
        if(pat[i] == union_all_samples) {
            assert(0.0 == mat[i * nDataFiles + j]);
            continue;
        }

        char lbl[100];
        patLbl(sizeof(lbl), lbl, pat[i], &lndx);
        printf("%-10s", lbl);
        for(j = 0; j < nDataFiles; ++j)
            printf(" %13.10lf", mat[i * nDataFiles + j]);
        putchar('\n');
    }
    fflush(stdout);

    free(datafname);
    if(legofname)
        free(legofname);

    return 0;
}
