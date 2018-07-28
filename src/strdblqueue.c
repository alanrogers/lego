/**
 * @file strdblqueue.c
 * @author Daniel R. Tabin and Alan R. Rogers
 * @brief Functions for Composite Likelihood Information Criterion.
 * @copyright Copyright (c) 2018, Alan R. Rogers
 * <rogers@anthro.utah.edu>. This file is released under the Internet
 * Systems Consortium License, which can be found in file "LICENSE".
 */

#include "hessian.h"
#include "misc.h"
#include "strdblqueue.h"
#include <stdbool.h>

// Push a value onto the tail of the queue. Return pointer to new
// head. Example:
//
// StrDblQueue *queue=NULL;
// queue = StrDblQueue_push(queue, "name1", 1.0);
// queue = StrDblQueue_push(queue, "name2", 2.0);
StrDblQueue *StrDblQueue_push(StrDblQueue *self, const char *str, double val) {
    if(self != NULL) {
        self->next = StrDblQueue_push(self->next, str, val);
        return self;
    }
    StrDblQueue *new = malloc(sizeof(StrDblQueue));
    CHECKMEM(new);
    new->strdbl.val = val;
    int status = snprintf(new->strdbl.str, sizeof(new->strdbl.str), "%s", str);
    if(status > sizeof(new->strdbl.str)) {
        fprintf(stderr, "%s:%d: buffer overflow\n", __FILE__, __LINE__);
        exit(EXIT_FAILURE);
    }
    new->next = NULL;
    return new;
}

// Pop a value off the head of the queue. Return pointer to new
// head. Example:
//
// StrDblQueue *queue=NULL;
// queue = StrDblQueue_push(queue, "name1", 1.0);
// queue = StrDblQueue_push(queue, "name2", 2.0);
//
// StrDbl x;            NOTE: Make sure this is not NULL
// queue = StrDblQueue_pop(queue, &x);  // x={"name1", 1.0}
// queue = StrDblQueue_pop(queue, &x);  // x={"name2", 2.0}
StrDblQueue *StrDblQueue_pop(StrDblQueue *self, StrDbl *strdbl) {
    if(self==NULL)
        return NULL;
    assert(strdbl != NULL);
    strdbl->val = self->strdbl.val;
    strcpy(strdbl->str, self->strdbl.str);
    StrDblQueue *next = self->next;
    free(self);
    return next;
}

// //get a strdbl
// // NOTE: This is inefficient, we should work on making this faster
//
// StrDbl *StrDblQueue_get(StrDblQueue *self, StrDbl *strdbl, int index) {
//     if(self==NULL)
//         return NULL;
//
//     StrDblQueue *temp;
//
//     for (int i = 0; i < index; i++)
//       temp = temp->next;
//
//     strdbl = &(temp->strdbl);
//     return strdbl;
// }

int StrDblQueue_length(StrDblQueue *self) {
    if(self==NULL)
        return 0;
    return 1 + StrDblQueue_length(self->next);
}

StrDblQueue *StrDblQueue_free(StrDblQueue *self) {
    if(self) {
        self->next = StrDblQueue_free(self->next);
        free(self);
    }
    return NULL;
}

void StrDblQueue_print(StrDblQueue *self, FILE *fp) {
    while(self) {
        fprintf(fp,"%s = %lg\n", self->strdbl.str, self->strdbl.val);
        self = self->next;
    }
}

/**
 * Compare the str fields in two StrDblQueue objects. Return -1, 0, or 1
 * if the lhs is less than, equal to, or greater than rhs.
 */
int StrDblQueue_compare(StrDblQueue *lhs, StrDblQueue *rhs) {
    while(lhs && rhs) {
        int cmp = strcmp(lhs->strdbl.str, rhs->strdbl.str);
        if(cmp)
            return cmp;
        lhs = lhs->next;
        rhs = rhs->next;
    }
    if(lhs)
        return 1;
    if(rhs)
        return -1;
    return 0;
}

// Parse a legofit output file. Return an object of type
// StrDblQueue, which contains the number of parameters, their names,
// and their values.
StrDblQueue *StrDblQueue_parseLegofit(const char *fname) {
    FILE *fp = fopen(fname, "r");
    if(fp==NULL) {
        fprintf(stderr,"%s:%d: can't read file \"%s\"\n",
                __FILE__,__LINE__,fname);
        exit(EXIT_FAILURE);
    }
    char buff[2000];
    int got_fitted=0;
    StrDblQueue *queue=NULL;
    while(1) {
        if(fgets(buff, sizeof buff, fp) == NULL) {
            break;
        }
        if(strchr(buff, '\n') == NULL && !feof(stdin)) {
            fprintf(stderr, "%s:%d: Buffer overflow. size=%zu\n",
                    __FILE__, __LINE__, sizeof(buff));
            exit(EXIT_FAILURE);
        }
        if(!got_fitted) {
            if(strncmp("Fitted", buff, 6) == 0)
                got_fitted=1;
            continue;
        }else if(got_fitted) {
            if(NULL != strstr(buff, "constrained")) {
                got_fitted = 0;
                continue;
            }
        }
        char *valstr = buff;
        char *name = strsep(&valstr, "=");
        if(name==NULL || valstr==NULL)
            continue;
        name = stripWhiteSpace(name);
        valstr = stripWhiteSpace(valstr);
        queue=StrDblQueue_push(queue, name, strtod(valstr, NULL) );
    }
    assert(StrDblQueue_length(queue) > 0);
    return queue;
}

// Parse a data file. Return an object of type
// StrDblQueue, which contains the number of parameters, their names,
// and their values.
StrDblQueue *StrDblQueue_parseSitPat(const char *fname) {
    FILE *fp = fopen(fname, "r");
    if(fp==NULL) {
        fprintf(stderr,"%s:%d: can't read file \"%s\"\n",
                __FILE__,__LINE__,fname);
        exit(EXIT_FAILURE);
    }
    char buff[2000];
    bool got_sitepat = false;
    StrDblQueue *queue=NULL;
    while(1) {
        if(fgets(buff, sizeof buff, fp) == NULL) {
            break;
        }
        if(strchr(buff, '\n') == NULL && !feof(stdin)) {
            fprintf(stderr, "%s:%d: Buffer overflow. size=%zu\n",
                    __FILE__, __LINE__, sizeof(buff));
            exit(EXIT_FAILURE);
        }
        if(!got_sitepat) {
            char* no_spaces_buff = stripInternalWhiteSpace(buff);
            if(strncmp("#SitePat", no_spaces_buff, 8) == 0)
                got_sitepat=true;
            continue;
        }

        char* temp = buff;
        char* name = strsep(&temp, " ");
        char* valstr = strsep(&temp, " ");

        if(name==NULL || valstr==NULL)
            continue;
        name = stripWhiteSpace(name);
        valstr = stripWhiteSpace(valstr);
        queue=StrDblQueue_push(queue, name, strtod(valstr, NULL) );
    }
    return queue;
}

void StrDblQueue_normalize(StrDblQueue* self){
  int length = StrDblQueue_length(self);
  double total = 0;
  StrDblQueue* temp;

  for(temp = self; temp; temp = temp->next){
    total += temp->strdbl.val;
  }

  temp = self;
  for(int i = 0; i < length; i++){
    temp->strdbl.val /= total;
    temp = temp->next;
  }
}

// Calculate mean squared deviation between the values of
// two StrDblQueue objects;
double StrDblQueue_msd(const StrDblQueue *a, const StrDblQueue *b) {
    const StrDblQueue *ia = a;
    const StrDblQueue *ib = b;
    double x, msd=0.0;
    int n=0;

    while(ia!=NULL && ib!=NULL) {
        StrDbl sda = ia->strdbl;
        StrDbl sdb = ib->strdbl;
        if(0 != strcmp(sda.str, sdb.str)) {
            fprintf(stderr, "%s:%s:%d: inconsistent strings: %s != %s\n",
                    __FILE__,__func__,__LINE__,
                    sda.str, sdb.str);
            exit(EXIT_FAILURE);
        }
        x = sda.val - sdb.val;
        msd += x*x;
        ++n;
        ia = ia->next;
        ib = ib->next;
    }
    if(ia || ib) {
        fprintf(stderr, "%s:%s:%d: queues are of unequal length\n",
                __FILE__,__func__,__LINE__);
        exit(EXIT_FAILURE);
    }
    return msd/n;
}

// On input, nfiles and npar are the number of rows and columns in
// the input data matrix "array". The npar X npar matrix "cov" should
// be allocated in the calling function.
//
// On return, cov[i][j] is the covariance between the i'th and j'th
// columns of "array".

void make_covar_matrix(int nfiles, int npar, double array[nfiles][npar],
                      gsl_matrix *cov){
   assert(npar == cov->size1);
   assert(npar == cov->size2);
   double param_averages[npar];
   double covar_sum;
   int i, j, k;

   for(i = 0; i < npar; i++){
       param_averages[i] = 0;
       for (j = 0; j < nfiles; j++){
           param_averages[i] += array[j][i];
       }
       param_averages[i] /= nfiles;
   }

   for(i = 0; i < npar; i++){
       for (j = 0; j < npar; j++){
           covar_sum = 0;
           for (k = 0; k < nfiles; k++){
               covar_sum += (array[k][j] - param_averages[j])
                   * (array[k][i] - param_averages[i]);
           }
           gsl_matrix_set(cov, i, j, covar_sum/nfiles);
       }
   }
}