/**
@file booma.c
@page booma
@author Alan R. Rogers and Daniel R. Tabin
@brief Bootstrap model averaging

# `booma`: bootstrap model averaging

Bootstrap model averaging was proposed by Buckland et al (Biometrics,
53(2):603-618). It can be used with weights provided by any method of
model selection, including @ref bepe "bepe" and @ref clic
"clic". Model selection is applied to the real data and also to a set
of bootstrap replicates. The weight, \f$w_i\f$ of the i'th model is
the fraction of these data sets for which the i'th model wins. In
other words, it is the fraction of data sets for which the i'th model
has the smallest information criterion.

The model-averaged estimator of a parameter, \f$\theta\f$, is the
average across models, weighted by \f$w_i\f$, of the model-specific
estimates of \f$\theta\f$. Models in which \f$\theta\f$ does not
appear are omitted from the weighted average.

To construct confidence intervals, we average across models within
each bootstrap replicate to obtain a bootstrap distribution of
model-averaged estimates.

Usage: booma <m1.msc> ... <mK.msc> -F <m1.flat> ... <mK.flat>

Here, the "mX" arguments refer to model "X". The "msc" suffix stands
for "model selection criterion". There are currently two options: @ref
bepe "bepe" and @ref clic "clic". Thus, the first command-line
argument might look like either "m1.bepe" or "m1.clic".

In either case, the "msc" files consist (apart from sharp-delimited
comments) of two columns. The first column gives the model selection
criterion, and the second column names the data file to which that
criterion refers. The first row should refer to the real data and the
remaining rows to bootstrap replicates. Model selection criteria are
defined so that low numbers indicate preferred models. I
will refer to these numbers as "badness" values.

After the `-F` argument comes a list of files, each of which can be
generated by @ref flatfile "flatfile.py". There must be a `.flat` file
for each model, so the number of `.flat` files should equal the number
of `.bepe` files. The first row of a `.flat` file is a header and
consists of column labels. Each column refers to a different
parameter, and the column labels are the names of these
parameters. The various `.flat` files need not agree about the number
of parameters or about the order of the parameters they share. But
shared parameters must have the same name in each `.flat` file.

After the header, each row in a `.flat` file refers to a different
data set. The first row after the header refers to the real data. Each
succeeding row refers to a bootstrap replicate. The number of rows
(excluding comments and the header) should agree with the numbers of
rows in the `.bepe` or `.clic` files.

In all types of input files, comments begin with a sharp character
and are ignored.

When `booma` runs, the first step is to calculate model weights,
\f$w_{i}\f$, where \f$i\f$ runs across models. The value of
\f$w_{i}\f$ is the fraction data sets (i.e. of rows in the `.bepe`
files) for which \f$i\f$ is the best model (i.e. the one with the
lowest badness value. If the best score is shared by several models,
they receive equal weights.

In the next step, `booma` averages across models to obtain a
model-averaged estimate of each parameter. This is done separately for
each data set: first for the real data and then for each bootstrap
replicate. Some parameters may be missing from some models. In this
case, the average runs only across models that include the parameter,
and the weights are re-normalized so that they sum to 1 within this
reduced set of models. If a parameter is present only in models with
weight zero, its model-averaged value is undefined and prints as "nan"
(not a number).

Finally, the program uses the bootstrap distribution of model-averaged
parameter estimates to construct a 95% confidence interval for each
parameter.

The program produces two output files. The first of these is written
to standard output and has the same form as the output of \ref
bootci "bootci.py". The first column consists of parameter names and the
2nd of model-averaged parameter estimates. The 3rd and 4th columns are
the lower and upper bounds of the confidence intervals.

The program also writes a file in the format of `ref flatfile
"flatfile.py". There is a header listing parameter labels. After the
header, row *i* gives the model-averaged estimate of each parameter
for the *i*th bootstrap replicate.

@copyright Copyright (c) 2018, Alan R. Rogers
<rogers@anthro.utah.edu>. This file is released under the Internet
Systems Consortium License, which can be found in file "LICENSE".
*/

#include "strdblqueue.h"
#include "strint.h"
#include "tokenizer.h"
#include "misc.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define LINESIZE 65536

typedef struct ModSelCrit {
    int dim;
    double *c;                  // c[i] is the criterion for the i'th data set
    char **fname;               // fname[i] points to name of i'th data set
} ModSelCrit;

// Data from one file as produced by flatfile.py
typedef struct ModPar {
    int nrows, ncols;
    StrInt *parndx;             // hash table to relate parameter names to
    // indices.
    double *par;                // par[i*ncols + j] is i'th par in j'th data set
    char *fname;                // name of input file
} ModPar;

// Linked list of parameter names
#define MAXNAME 100
typedef struct ParNameLst {
    struct ParNameLst *next;
    char name[MAXNAME];
} ParNameLst;

void usage(void);
ModSelCrit *ModSelCrit_new(const char *fname);
void ModSelCrit_free(ModSelCrit * self);
int ModSelCrit_compare(ModSelCrit * x, ModSelCrit * y, int isBepe);
int ModSelCrit_dim(ModSelCrit * self);
double ModSelCrit_badness(ModSelCrit * self, int ndx);
ModPar *ModPar_new(const char *fname, ParNameLst ** namelist);
void ModPar_free(ModPar * self);
int ModPar_exists(ModPar * self, const char *parname);
double ModPar_value(ModPar * self, int row, const char *parname);
int ModPar_nrows(ModPar * self);
int ModPar_ncols(ModPar * self);
ParNameLst *ParNameLst_new(const char *name, ParNameLst * next);
ParNameLst *ParNameLst_insert(ParNameLst * self, const char *name);
void ParNameLst_free(ParNameLst * self);
int ParNameLst_exists(ParNameLst * self, const char *name);
void ParNameLst_print(const ParNameLst * self, FILE * fp);
unsigned ParNameLst_size(ParNameLst * self);
int strbepe(const char *s);

const char *usageMsg =
    "Usage: booma <m1.msc> ... <mK.msc> -F <m1.flat> ... <mK.flat>\n"
    "\n"
    "Here, the \"mX\" arguments refer to model \"X\". The \"msc\" suffix\n"
    "stands for \"model selection criterion\". There are currently two\n"
    "options: \"bepe\" and \"clic\". Thus, the first command-line\n"
    "argument might look like either \"m1.bepe\" or \"m1.clic\".\n\n"
    "In either case, the \"msc\" files consist (apart from sharp-delimited\n"
    "comments) of two columns. The first column gives the model selection\n"
    "criterion, and the second column names the data file to which that\n"
    "criterion refers. The first row should refer to the real data and the\n"
    "remaining rows to bootstrap replicates. Model selection criteria are\n"
    "defined so that low numbers indicate preferred models.\n\n"
    "After the \"-F\" argument comes a list of files, each of which can be\n"
    "generated by \"flatfile.py\". There must be a \".flat\" file\n"
    "for each model, so the number of \".flat\" files should equal the\n"
    "number of \".bepe\" files. The first row of a \".flat\" file is a header\n"
    "and consists of column labels. Each column refers to a different\n"
    "parameter, and the column labels are the names of these parameters.\n"
    "The various \".flat\" files need not agree about the number of\n"
    "parameters or about the order of the parameters they share. But\n"
    "shared parameters must have the same name in each \".flat\" file.\n\n"
    "After the header, each row in a \".flat\" file refers to a different\n"
    "data set. The first row after the header refers to the real data. Each\n"
    "succeeding row refers to a bootstrap replicate. The number of rows\n"
    "(excluding comments and the header) should agree with the numbers of\n"
    "rows in the \".bepe\" files.\n\n"
    "In both types of input files, comments begin with a sharp character\n"
    "and are ignored.\n";

void usage(void) {
    fputs(usageMsg, stderr);
    exit(EXIT_FAILURE);
}

/// Return 1 if string ends with ".bepe", 0 otherwise
int strbepe(const char *s) {
    int len = strlen(s);
    if(len < 5)
        return 0;
    s += len-5;
    if(strcmp(s, ".bepe") == 0)
        return 1;
    return 0;
}

/// Construct a new object of type ModSelCrit by parsing a file.
/// Return NULL on failure.
ModSelCrit *ModSelCrit_new(const char *fname) {
    FILE *fp = fopen(fname, "r");
    if(fp == NULL) {
        fprintf(stderr, "%s:%d: can't read file \"%s\"\n",
                __FILE__, __LINE__, fname);
        exit(EXIT_FAILURE);
    }
    char buff[LINESIZE];

    // 1st pass counts lines
    int dim = 0;
    while(1) {
        if(fgets(buff, sizeof buff, fp) == NULL) {
            break;
        }
        if(strchr(buff, '\n') == NULL && !feof(stdin)) {
            fprintf(stderr, "%s:%d: Buffer overflow. size=%zu\n",
                    __FILE__, __LINE__, sizeof(buff));
            exit(EXIT_FAILURE);
        }
        char *next = stripWhiteSpace(buff);
        assert(next);
        if(*next == '#' || strlen(next) == 0)
            continue;
        ++dim;
    }
    if(dim==0) {
        fprintf(stderr,"%s:%s:%d: can't parse \"%s\" as an msc file\n",
                __FILE__,__func__,__LINE__,fname);
        exit(EXIT_FAILURE);
    }

    ModSelCrit *msc = malloc(sizeof(ModSelCrit));
    CHECKMEM(msc);

    msc->dim = dim;
    msc->c = malloc(dim * sizeof(msc->c[0]));
    msc->fname = malloc(dim * sizeof(msc->fname[0]));
    if(msc->c == NULL || msc->fname == NULL) {
        fprintf(stderr, "%s:%d: bad malloc\n", __FILE__, __LINE__);
        exit(EXIT_FAILURE);
    }

    int line = 0;
    rewind(fp);
    while(1) {
        if(fgets(buff, sizeof buff, fp) == NULL)
            break;
        if(strchr(buff, '\n') == NULL && !feof(stdin)) {
            fprintf(stderr, "%s:%d: Buffer overflow. size=%zu\n",
                    __FILE__, __LINE__, sizeof(buff));
            exit(EXIT_FAILURE);
        }
        char *next = stripWhiteSpace(buff);
        assert(next);
        if(*next == '#' || strlen(next) == 0)
            continue;
        char *valstr = strsep(&next, " \t");
        next = stripWhiteSpace(next);
        if(valstr == NULL || next == NULL || strlen(next) == 0) {
            fprintf(stderr, "%s:%d: can't parse file %s\n",
                    __FILE__, __LINE__, fname);
            exit(EXIT_FAILURE);
        }
        assert(line < msc->dim);
        msc->c[line] = strtod(valstr, NULL);
        msc->fname[line] = strdup(next);
        if(msc->fname[line] == NULL) {
            fprintf(stderr, "%s:%d: bad strdup\n", __FILE__, __LINE__);
            exit(EXIT_FAILURE);
        }
        ++line;
    }
    assert(line == msc->dim);
    fclose(fp);
    return msc;
}

void ModSelCrit_free(ModSelCrit * self) {
    int i;
    for(i = 0; i < self->dim; ++i) {
        if(self->fname[i])
            free(self->fname[i]);
    }
    free(self->fname);
    free(self->c);
    free(self);
}

// Compares dimensions and filenames. Return 0 if x and y agree.
int ModSelCrit_compare(ModSelCrit * x, ModSelCrit * y, int isBepe) {
    int i, diff;

    diff = x->dim - y->dim;
    if(diff) {
        fprintf(stderr,"%s:%s:%d: inconsistent dimensions: %d != %d\n",
                __FILE__,__func__,__LINE__, x->dim, y->dim);
        return diff;
    }

    // data file names must agree only if the model selection criterion
    // is bepe.
    for(i = 0; isBepe && i < x->dim; ++i) {
        diff = strcmp(x->fname[i], y->fname[i]);
        if(diff) {
            fprintf(stderr,"%s:%s:%d: inconsistent data file names\n"
                    " data file %d: %s != %s\n",
                    __FILE__,__func__,__LINE__,
                    i, x->fname[i], y->fname[i]);
            return diff;
        }
    }
    return 0;
}

int ModSelCrit_dim(ModSelCrit * self) {
    return self->dim;
}

double ModSelCrit_badness(ModSelCrit * self, int ndx) {
    assert(ndx < self->dim);
    return self->c[ndx];
}

/// ModPar constructor. Argument is the name of a file in the format
/// produced by flatfile.py.
ModPar *ModPar_new(const char *fname, ParNameLst ** namelist) {
    FILE *fp = fopen(fname, "r");
    if(fp == NULL) {
        fprintf(stderr, "%s:%d: can't read file \"%s\"\n",
                __FILE__, __LINE__, fname);
        exit(EXIT_FAILURE);
    }
    char buff[LINESIZE];
    Tokenizer *tkz = Tokenizer_new(100);    // room for 100 parameters

    ModPar *self = malloc(sizeof(ModPar));
    CHECKMEM(self);
    self->parndx = StrInt_new();
    CHECKMEM(self->parndx);
    self->fname = strdup(fname);
    CHECKMEM(self->fname);

    // 1st pass counts rows and columns. Creates hash map.
    int i, j, nrows = 0, ncols = 0, ntokens;
    while(1) {
        if(fgets(buff, sizeof buff, fp) == NULL)
            break;

        if(strchr(buff, '\n') == NULL && !feof(stdin)) {
            fprintf(stderr, "%s:%d: Buffer overflow. size=%zu\n",
                    __FILE__, __LINE__, sizeof(buff));
            exit(EXIT_FAILURE);
        }
        if(*buff == '#')
            continue;
        ntokens = Tokenizer_split(tkz, buff, " \t");
        ntokens = Tokenizer_strip(tkz, " \n\t");
        if(ncols == 0) {
            ncols = ntokens;
            self->ncols = ncols;
            // Create hash table mapping parameter names to indices.
            // Also add parameter names to linked list
            for(j = 0; j < ncols; ++j) {
                const char *name = Tokenizer_token(tkz, j);
                StrInt_insert(self->parndx, name, j);
                *namelist = ParNameLst_insert(*namelist, name);
            }
        } else {
            if(ncols != ntokens) {
                fprintf(stderr,
                        "%s:%d: inconsistent row lengths in file %s\n",
                        __FILE__, __LINE__, fname);
                fprintf(stderr, "  current line has %d tokens\n", ntokens);
                fprintf(stderr, "  previous lines had %d\n", ncols);
                exit(EXIT_FAILURE);
            }
            ++nrows;
        }
    }

    self->nrows = nrows;
    self->par = malloc(nrows * ncols * sizeof(double));
    CHECKMEM(self->par);
    self->fname = malloc(nrows * sizeof(char *));
    CHECKMEM(self->fname);
    rewind(fp);

    // 2nd pass puts parameter values into array
    ncols = 0;
    i = 0;
    while(1) {
        if(fgets(buff, sizeof buff, fp) == NULL) {
            break;
        }
        if(strchr(buff, '\n') == NULL && !feof(stdin)) {
            fprintf(stderr, "%s:%d: Buffer overflow. size=%zu\n",
                    __FILE__, __LINE__, sizeof(buff));
            exit(EXIT_FAILURE);
        }
        if(*buff == '#')
            continue;
        ntokens = Tokenizer_split(tkz, buff, " \t");
        ntokens = Tokenizer_strip(tkz, " \n\t");
        if(ntokens == 0)
            continue;
        if(ncols == 0) {
            ncols = ntokens;
            assert(ncols == self->ncols);
        } else {
            assert(self->ncols == ntokens);
            for(j = 0; j < ntokens; ++j) {
                double x = strtod(Tokenizer_token(tkz, j), NULL);
                self->par[i * self->ncols + j] = x;
            }
            ++i;
        }
    }
    assert(i == nrows);

    return self;
}

void ModPar_free(ModPar * self) {
    StrInt_free(self->parndx);
    free(self->par);
    free(self->fname);
    free(self);
}

/// Return 1 if parname exists w/i this model; 0 otherwise.
int ModPar_exists(ModPar * self, const char *parname) {
    return StrInt_exists(self->parndx, parname);
}

double ModPar_value(ModPar * self, int row, const char *parname) {
    errno = 0;
    int col = StrInt_get(self->parndx, parname);
    if(errno) {
        fprintf(stderr,
                "%s:%d: ERR: bad parameter name %s\n",
                __FILE__, __LINE__, parname);
        exit(EXIT_FAILURE);
    }
    return self->par[row * self->ncols + col];
}

int ModPar_nrows(ModPar * self) {
    return self->nrows;
}

int ModPar_ncols(ModPar * self) {
    return self->ncols;
}

/// ParNameLst constructor
ParNameLst *ParNameLst_new(const char *name, ParNameLst * next) {
    ParNameLst *new = malloc(sizeof(*new));
    CHECKMEM(new);

    new->next = next;
    int status = snprintf(new->name, sizeof new->name, "%s", name);
    if(status >= MAXNAME) {
        fprintf(stderr, "%s:%s:%d: Buffer overflow. MAXNAME=%d, name=%s\n",
                __FILE__, __func__, __LINE__, MAXNAME, name);
        free(new);
        exit(EXIT_FAILURE);
    }

    return new;
}

/// Return number of links in list
unsigned ParNameLst_size(ParNameLst * self) {
    if(self == NULL)
        return 0u;
    return 1u + ParNameLst_size(self->next);
}

/// Free linked list of ParNameLst objects
void ParNameLst_free(ParNameLst * self) {
    if(self == NULL)
        return;
    ParNameLst_free(self->next);
    free(self);
}

/// Insert a new name. Do nothing if name already exists.
ParNameLst *ParNameLst_insert(ParNameLst * self, const char *name) {
    if(self == NULL)
        return ParNameLst_new(name, NULL);

    int diff = strcmp(name, self->name);
    if(diff == 0) {
        return self;
    } else if(diff > 0) {
        self->next = ParNameLst_insert(self->next, name);
        return self;
    } else
        return ParNameLst_new(name, self);
}

/// Return 1 if name is present in list, 0 otherwise.
int ParNameLst_exists(ParNameLst * self, const char *name) {
    if(self == NULL) {
        return 0;
    }

    int diff = strcmp(name, self->name);
    if(diff == 0)
        return 1;
    else if(diff > 0)
        return ParNameLst_exists(self->next, name);
    else {
        assert(diff < 0);
        return 0;
    }
}

/// Print linked list of ParNameLst objects
void ParNameLst_print(const ParNameLst * self, FILE * fp) {
    if(self == NULL) {
        putc('\n', fp);
        return;
    }
    fprintf(fp, " %s", self->name);
    ParNameLst_print(self->next, fp);
}

/*
 * This file has two main functions. The first compiles if "TEST" is
 * defined provides a unit test for the functions in this file. The
 * second "main" compiles if "TEST" is not defined it produces the booma
 * executable.
 */
#ifdef TEST
#  ifdef NDEBUG
#    error "Unit tests must be compiled without -DNDEBUG flag"
#  endif

const char *mscInput =
    "# comment\n" "\n" "# comment\n" "0.01 foo\n" "0.02 bar\n";

const char *flatInput =
    "# comment\n" "\n" "# comment\n" "par1 par2\n" "1.0  2.0\n" "3e+0 4\n";

int main(int argc, char **argv) {
    int verbose = 0;
    if(argc > 1) {
        if(argc != 2 || 0 != strcmp(argv[1], "-v")) {
            fprintf(stderr, "usage: xstrint [-v]\n");
            exit(EXIT_FAILURE);
        }
        verbose = 1;
    }

    FILE *fp;

    assert(1 == strbepe("asdf.bepe"));
    assert(1 == strbepe(".bepe"));
    assert(0 == strbepe("aa.bep"));
    unitTstResult("strbepe", "OK");

    const char *mscFile = "tst.msc";
    fp = fopen(mscFile, "w");
    assert(fp);
    fputs(mscInput, fp);
    fclose(fp);
    ModSelCrit *msc = ModSelCrit_new(mscFile);
    ModSelCrit *msc2 = ModSelCrit_new(mscFile);
    assert(ModSelCrit_compare(msc, msc2) == 0);
    msc2->fname[0][0] += 1;
    assert(ModSelCrit_compare(msc, msc2) != 0);
    assert(ModSelCrit_dim(msc) == 2);
    assert(ModSelCrit_badness(msc, 0) == 0.01);
    assert(ModSelCrit_badness(msc, 1) == 0.02);
    ModSelCrit_free(msc);
    ModSelCrit_free(msc2);
    remove(mscFile);
    unitTstResult("ModSelCrit", "OK");

    ParNameLst *pnl = NULL, *node;
    assert(ParNameLst_size(pnl) == 0);
    pnl = ParNameLst_insert(pnl, "george");
    assert(ParNameLst_size(pnl) == 1);
    pnl = ParNameLst_insert(pnl, "frank");
    assert(ParNameLst_size(pnl) == 2);
    pnl = ParNameLst_insert(pnl, "alfred");
    assert(ParNameLst_size(pnl) == 3);
    node = pnl;
    assert(node);
    assert(strcmp(node->name, "alfred") == 0);
    node = node->next;
    assert(node);
    assert(strcmp(node->name, "frank") == 0);
    node = node->next;
    assert(node);
    assert(strcmp(node->name, "george") == 0);
    node = node->next;
    assert(node == NULL);
    assert(ParNameLst_exists(pnl, "alfred"));
    assert(ParNameLst_exists(pnl, "frank"));
    assert(ParNameLst_exists(pnl, "george"));
    assert(!ParNameLst_exists(pnl, "notthere"));
    if(verbose)
        ParNameLst_print(pnl, stdout);
    ParNameLst_free(pnl);
    unitTstResult("ParNameLst", "OK");

    const char *flatFile = "tst.flat";
    fp = fopen(flatFile, "w");
    assert(fp);
    fputs(flatInput, fp);
    fclose(fp);
    pnl = NULL;
    ModPar *mp = ModPar_new(flatFile, &pnl);
    assert(ModPar_exists(mp, "par1"));
    assert(ModPar_exists(mp, "par2"));
    assert(!ModPar_exists(mp, "par3"));
    assert(ParNameLst_size(pnl) == 2);
    assert(ModPar_nrows(mp) == 2);
    assert(ModPar_ncols(mp) == 2);
    assert(ModPar_value(mp, 0, "par1") == 1.0);
    assert(ModPar_value(mp, 0, "par2") == 2.0);
    assert(ModPar_value(mp, 1, "par1") == 3.0);
    assert(ModPar_value(mp, 1, "par2") == 4.0);
    remove(flatFile);
    unitTstResult("ModPar", "OK");
    return 0;
}
#else
int main(int argc, char **argv) {
    time_t currtime = time(NULL);
    int i, j, k, nmodels = 0, gotDashF = 0;

    hdr("booma: bootstrap model average");
#  if defined(__DATE__) && defined(__TIME__)
    printf("# Program was compiled: %s %s\n", __DATE__, __TIME__);
#  endif
    printf("# Program was run: %s", ctime(&currtime));
    printf("# cmd:");
    for(i = 0; i < argc; ++i)
        printf(" %s", argv[i]);
    putchar('\n');
    fflush(stdout);

    {
        int nflat = 0;

        // first pass through arguments: count files
        for(i = 1; i < argc; i++) {
            if(argv[i][0] == '-') {
                if(strcmp(argv[i], "-F") == 0) {
                    gotDashF = 1;
                    continue;
                } else
                    usage();
            }
            if(gotDashF)
                ++nflat;
            else
                ++nmodels;
        }
        if(nmodels != nflat) {
            fprintf(stderr, "%s:%d\n"
                    " Inconsistent files count."
                    " %d model selection files != %d flat files\n",
                    __FILE__, __LINE__, nmodels, nflat);
            usage();
        }
    }

    if(nmodels < 2)
        usage();

    const char *mscnames[nmodels];
    const char *flatnames[nmodels];

    // 2nd pass through arguments: put file names in arrays
    gotDashF = 0;
    j = 0;
    for(i = 1; i < argc; i++) {
        if(argv[i][0] == '-') {
            if(strcmp(argv[i], "-F") == 0) {
                gotDashF = 1;
                j = 0;
                continue;
            } else
                usage();
        }
        if(gotDashF)
            flatnames[j++] = argv[i];
        else
            mscnames[j++] = argv[i];
    }

    int isBepe = strbepe(mscnames[0]);
    for(i=1; i<nmodels; ++i) {
        if(isBepe != strbepe(mscnames[i])) {
            fprintf(stderr,"%s:%d: inconsistent MSC file types.\n",
                    __FILE__,__LINE__);
            if(isBepe)
                fprintf(stderr," %s is a .bepe file; %s isn't\n",
                        mscnames[0], mscnames[i]);
            else
                fprintf(stderr," %s is a .bepe file; %s isn't\n",
                        mscnames[i], mscnames[0]);
            exit(EXIT_FAILURE);
        }
    }

    ModSelCrit *msc[nmodels];

    // Parse msc files and check for consistency
    for(i = 0; i < nmodels; ++i) {
        msc[i] = ModSelCrit_new(mscnames[i]);
        if(i > 0) {
            if(ModSelCrit_compare(msc[0], msc[i], isBepe)) {
                fprintf(stderr, "%s:%d: inconsistent files: %s and %s\n",
                        __FILE__, __LINE__, mscnames[0], mscnames[i]);
                exit(EXIT_FAILURE);
            }
        }
    }

    int nrows = ModSelCrit_dim(msc[0]);
    ParNameLst *parnames = NULL;
    ModPar *modpar[nmodels];

    // Flat files should have the same number of rows as msc
    // files--one per data set. They have a column for each parameter,
    // and the parameters may not agree across files. This section
    // creates for each file an object of type ModPar, which allows
    // access to the parameter values by row index and parameter name.
    // It also constructs an ordered linked list (parnames) which
    // includes the names of all parameters.
    for(i = 0; i < nmodels; ++i) {
        modpar[i] = ModPar_new(flatnames[i], &parnames);
        CHECKMEM(modpar[i]);
        if(ModPar_nrows(modpar[i]) != nrows) {
            fprintf(stderr, "%s:%d: file \"%s\" has %d rows;"
                    " previous files had %d\n",
                    __FILE__, __LINE__, flatnames[i],
                    ModPar_nrows(modpar[i]), nrows);
            exit(EXIT_FAILURE);
        }
    }

    // Calculate weights, w[i].  w[i] is the fraction of data sets for
    // which i is the best model. If the best score is shared by several
    // models, each of them gets an equal share of the weight.
    double w[nmodels];
    memset(w, 0, sizeof w);
    for(i = 0; i < nrows; ++i) {
        int jbest = 0, nbest = 0;
        double best = DBL_MAX;
        for(j = 0; j < nmodels; ++j) {
            double badness = ModSelCrit_badness(msc[j], i);
            if(badness < best) {
                best = badness;
                jbest = j;
                nbest = 1;
            }else if(badness == best)
                ++nbest;
        }
        assert(nbest >= 1);
        if(nbest == 1) {
            // no ties: all weight goes to one model
            w[jbest] += 1.0;
        }else{
            // distribute weight across ties. jbest is the index
            // of the first of the tied models.
            double share = 1.0/nbest;
            for(j = jbest; j < nmodels; ++j) {
                double badness = ModSelCrit_badness(msc[j], i);
                if(badness == best)
                    w[j] += share;
            }
        }
    }
    for(j = 0; j < nmodels; ++j)
        w[j] /= nrows;

    // Echo weights and input files
    printf("#%15s %15s %15s\n", "Weight", "MSC_file", "Flat_file");
    for(i = 0; i < nmodels; ++i)
        printf("#%15.10lg %15s %15s\n", w[i], mscnames[i], flatnames[i]);
    putchar('\n');

    // number of parameters, pooled across models
    int npar = ParNameLst_size(parnames);

    // avg[i][j] is average of j'th parameter for i'th data set
    double avg[nrows][npar];
    ParNameLst *node;

    for(i = 0; i < nrows; ++i) {
        // i indexes data sets
        for(j = 0, node = parnames; j < npar && node != NULL;
            ++j, node = node->next) {
            // j indexes parameters
            // par is the name of the current parameter
            const char *par = node->name;
            int exist[nmodels]; // 0 for models lacking par
            int nExist = 0;     // # models containing par
            double wsum = 0.0;
            for(k = 0; k < nmodels; ++k) {
                if(ModPar_exists(modpar[k], par)) {
                    // par exists in model k
                    ++nExist;
                    exist[k] = 1;
                    wsum += w[k];
                } else
                    exist[k] = 0;
            }
            avg[i][j] = 0.0;
            for(k = 0; k < nmodels; ++k) {
                if(!exist[k])
                    continue;
                double parval = ModPar_value(modpar[k], i, par);
                avg[i][j] += parval * w[k] / wsum;
            }
        }
        assert(j == npar);
        assert(node == NULL);
    }

    // Output matrix: rows are data sets; cols are parameters;
    // values are model-averaged estimates.
    printf("# Model-averaged parameter estimates\n");
    for(node = parnames; node != NULL; node = node->next)
        printf(" %s", node->name);
    putchar('\n');
    for(i = 0; i < nrows; ++i) {
        for(j = 0; j < npar; ++j)
            printf(" %0.10g", avg[i][j]);
        putchar('\n');
    }

    // Cleanup
    for(i = 0; i < nmodels; ++i)
        ModSelCrit_free(msc[i]);

    ParNameLst_free(parnames);

    for(i = 0; i < nmodels; ++i)
        ModPar_free(modpar[i]);
    return 0;
}
#endif
