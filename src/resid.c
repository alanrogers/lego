/**
@file resid.c
@page resid
@author Alan R. Rogers
@brief Calculate residuals

# `resid`: calculate residuals

This program reads a list of data files (containing site pattern
frequencies) and another list of .legofit files (containing estimates
as generated by `legofit`). The .legofit files should be listed in the
same order as the data files, so that the i'th .legofit file describes
the output generated from the i'th data file. The `resid` program uses
this input to calculate residuals--the difference between observed and
fitted site pattern frequencies--for each data set.

Optionally, the user can specify one or more remappings, each of which
tells `resid` to collapse several populations into a single population.

@copyright Copyright (c) 2019, Alan R. Rogers
<rogers@anthro.utah.edu>. This file is released under the Internet
Systems Consortium License, which can be found in file "LICENSE".
*/

#include "strdblqueue.h"
#include "misc.h"
#include "branchtab.h"
#include <strings.h>
#include <time.h>

// Function prototypes
void usage(void);

//vars
const char *usageMsg =
    "usage: resid [options] <realdat> <bdat1> <bdat2> ... -L <real.legofit>\n"
    " <b1.legofit> <b2.legofit> ... [-M c=a+b ...]\n" "\n"
    " where realdat is the real data, each \"bdat\" file is the data\n"
    " for one bootstrap or simulation replicate, and each \"b#.legofit\"\n"
    " file is the legofit output from the corresponding bootstrap replicate\n"
    " Must include at least one data file and one .legofit file.\n" "\n"
    "Options:\n"
    "   -h or --help   : print this message\n"
    "   -M c=a+b       : map populations a and b into a single symbol, c\n";

void usage(void) {
    fputs(usageMsg, stderr);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {

    time_t currtime = time(NULL);
    //    tipId_t collapse = 014; // collapse A and V

    hdr("resid: calculate residuals");
#if defined(__DATE__) && defined(__TIME__)
    printf("# Program was compiled: %s %s\n", __DATE__, __TIME__);
#endif
    printf("# Program was run: %s", ctime(&currtime));

    int i, j;
    int nfiles=0, nLegoFiles=0;
    int gotDashL = 0;

    for(i = 1; i < argc; i++) {
        if(argv[i][0] == '-') {
            if(strcmp(argv[i], "-L") == 0) {
                gotDashL = 1;
                continue;
            }else if(strcmp(argv[i], "-h") == 0 ||
                     strcmp(argv[i], "--help") == 0) {
                usage();
            }else{
                fprintf(stderr,"unknown flag argument: %s\n", argv[i]);
                usage();
            }
        }
        if(gotDashL)
            ++nLegoFiles;
        else
            ++nfiles;
    }
    if(nfiles != nLegoFiles) {
        fprintf(stderr, "%s:%d\n"
                " Inconsistent number of files!"
                " %d data files and %d legofit files\n",
                __FILE__, __LINE__, nfiles, nLegoFiles);
        usage();
    }

    if(nfiles < 1) {
        fprintf(stderr,"nfiles=%d; need at least 1\n", nfiles);
        usage();
    }

    const char *datafname[nfiles], *legofname[nfiles];
    gotDashL=0;
    j=0;
    for(i = 1; i < argc; i++) {
        if(argv[i][0] == '-') {
            if(strcmp(argv[i], "-L") == 0) {
                gotDashL = 1;
                assert(j==nfiles);
                j = 0;
            }
            continue;
        }
        if(gotDashL)
            legofname[j++] = argv[i];
        else
            datafname[j++] = argv[i];
    }
    assert(j==nfiles);

    putchar('\n');

    // Read data and legofit files into an arrays of queues
    StrDblQueue *data_queue[nfiles], *lego_queue[nfiles];
    for(i = 0; i < nfiles; ++i) {
        lego_queue[i] = StrDblQueue_parseSitePat(legofname[i]);
        data_queue[i] = StrDblQueue_parseSitePat(datafname[i]);
        StrDblQueue_normalize(lego_queue[i]);
        StrDblQueue_normalize(data_queue[i]);
        if(i==0) {
            checkConsistency(datafname[0], legofname[0],
                             data_queue[0], lego_queue[0]);
            continue;
        }
        checkConsistency(legofname[0], legofname[i],
                         lego_queue[0], lego_queue[i]);
        checkConsistency(datafname[0], legofname[i],
                         data_queue[0], data_queue[i]);
    }

    // number of site patterns and an array of patterns
    int npat = StrDblQueue_length(lego_queue[0]);
    j=0;
    char *pat[npat];

    // Create LblNdx object with an entry for each population
    tipId_t tid;
    LblNdx lndx;
    LblNdx_init(&lndx);
    StrDblQueue *sdq;
    for(sdq=data_queue[0]; sdq; sdq = sdq->next) {
        char *s = sdq->strdbl.str;
        if(j >= npat) {
            fprintf(stderr,"%s:%d: buffer overflow\n",__FILE__,__LINE__);
            exit(EXIT_FAILURE);
        }
        pat[j++] = strdup(s);
        while(1) {
            char *colon = strchr(s, ':');
            char buff[100];
            int len;
            if(colon == NULL) {
                tid = LblNdx_getTipId(&lndx, s);
                if(tid == 0)
                    LblNdx_addSamples(&lndx, 1u, s);
                break;
            }
            len = colon - s;
            int status = strnncopy(sizeof(buff), buff, len, s);
            if(status) {
                fprintf(stderr,"%s:%d: buffer overflow\n",
                        __FILE__,__LINE__);
                exit(EXIT_FAILURE);
            }
            tid = LblNdx_getTipId(&lndx, s);
            if(tid == 0)
                LblNdx_addSamples(&lndx, 1u, buff);
            s = colon+1;
        }
    }

    // resid[i*nfiles + j] is residual for pattern i in file j
    double mat[npat*nfiles];

    for(i=0; i<nfiles; ++i) {
        StrDbl strdbl;

        // Convert queues to BranchTab objects
        BranchTab *resid = BranchTab_new();
        while(data_queue[i] != NULL) {
            data_queue[i] = StrDblQueue_pop(data_queue[i], &strdbl);
            tid = LblNdx_getTipId(&lndx, strdbl.str);
            if(tid == 0) {
                fprintf(stderr,"%s:%d:unknown label (%s)\n",
                        __FILE__,__LINE__,strdbl.str);
                exit(EXIT_FAILURE);
            }
            BranchTab_add(resid, tid, strdbl.val);
        }
        BranchTab *fitted = BranchTab_new();
        while(lego_queue[i] != NULL) {
            lego_queue[i] = StrDblQueue_pop(lego_queue[i], &strdbl);
            tid = LblNdx_getTipId(&lndx, strdbl.str);
            if(tid == 0) {
                fprintf(stderr,"%s:%d:unknown label (%s)\n",
                        __FILE__,__LINE__,strdbl.str);
                exit(EXIT_FAILURE);
            }
            BranchTab_add(fitted, tid, strdbl.val);
        }

        // Normalize the BranchTabs
        if(BranchTab_normalize(resid))
            DIE("can't normalize empty BranchTab");
        if(BranchTab_normalize(fitted))
            DIE("can't normalize empty BranchTab");

        // calculate residuals for current pair of files
        BranchTab_minusEquals(resid, fitted);

        // store residual in matrix
        for(j=0; j < npat; ++j) {
            tid = LblNdx_getTipId(&lndx, pat[j]);
            mat[j*nfiles + i] = BranchTab_get(resid, tid);
        }
        BranchTab_free(resid);
        BranchTab_free(fitted);
    }

    // output
    printf("%-10s", "SitePat");
    for(j=0; j < nfiles; ++j) 
        printf(" %12.12s", legofname[j]);
    putchar('\n');
    for(i=0; i<npat; ++i) {
        printf("%-10s", pat[i]);
        for(j=0; j < nfiles; ++j) 
            printf(" %12.9lf", mat[i*nfiles + j]);
        putchar('\n');
    }
    putchar('\n');

    return 0;
}
