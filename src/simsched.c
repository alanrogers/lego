#include "simsched.h"
#include "misc.h"
#include <stdio.h>
#include <pthread.h>
#include <string.h>

typedef struct Stage Stage;

static Stage *Stage_append(Stage * self, long nOptItr,
                                         long nSimReps);
static Stage *Stage_dup(const Stage *self);
static Stage *Stage_free(Stage * self);
static Stage *Stage_popHead(Stage * self);
static inline long Stage_getSimReps(Stage * self);
static inline long Stage_getOptItr(Stage * self);

struct Stage {
    Stage *next;
    long          nOptItr, nSimReps;
};

struct SimSched {
    pthread_mutex_t lock;
    Stage *list;
};

// Append a new link to the end of the list. Return a pointer to the
// beginning of the list.
static Stage *Stage_append(Stage * self, long nOptItr,
                                         long nSimReps) {
    if(self == NULL) {
        Stage *new = malloc(sizeof(Stage));
        CHECKMEM(new);
        new->next = NULL;
        new->nOptItr = nOptItr;
        new->nSimReps = nSimReps;
        return new;
    }
    self->next = Stage_append(self->next, nOptItr, nSimReps);
    return self;
}

Stage *Stage_dup(const Stage *self) {
    if(self == NULL)
        return NULL;

    Stage *new = memdup(self, sizeof(Stage));
    CHECKMEM(new);

    new->next = Stage_dup(self->next);
    return new;
}

// Free the list and return NULL.
static Stage *Stage_free(Stage * self) {
    if(self == NULL)
        return NULL;
    self->next = Stage_free(self->next);
    free(self);
    return NULL;
}

// Delete head of the list and return a pointer
// to the new head.
static Stage *Stage_popHead(Stage * self) {
    if(self == NULL)
        return NULL;
    Stage *head = self->next;
    free(self);
    return head;
}

static inline long Stage_getSimReps(Stage * self) {
    if(self == NULL) {
        fprintf(stderr, "%s:%d: NULL Stage\n", __FILE__, __LINE__);
        exit(EXIT_FAILURE);
    }
    return self->nSimReps;
}

static inline long Stage_getOptItr(Stage * self) {
    if(self == NULL) {
        fprintf(stderr, "%s:%d: NULL Stage\n", __FILE__, __LINE__);
        exit(EXIT_FAILURE);
    }
    return self->nOptItr;
}

// Allocate a new SimSched with one stage.
SimSched   *SimSched_new(void) {
    SimSched   *self = malloc(sizeof(SimSched));
    CHECKMEM(self);

    self->list = NULL;

    int         status;
    if((status = pthread_mutex_init(&self->lock, NULL)))
        ERR(status, "init");

    return self;
}

int         SimSched_empty(const SimSched *self) {
    return self->list == NULL;
};

SimSched   *SimSched_dup(const SimSched *self) {
    if(self == NULL)
        return NULL;
    SimSched *new = SimSched_new();
    CHECKMEM(new);

    new->list = Stage_dup(self->list);
    return new;
}

// Append a stage to a SimSched.
void SimSched_append(SimSched * self, long nOptItr, long nSimReps) {
    int         status;

    status = pthread_mutex_lock(&self->lock);
    if(status)
        ERR(status, "lock");

    self->list = Stage_append(self->list, nOptItr, nSimReps);
    CHECKMEM(self->list);

    status = pthread_mutex_unlock(&self->lock);
    if(status)
        ERR(status, "unlock");
}

// Free a SimSched.
void SimSched_free(SimSched * self) {

    self->list = Stage_free(self->list);
    free(self);
}

long SimSched_getSimReps(SimSched * self) {
    int         status;
    long        nSimReps;

    status = pthread_mutex_lock(&self->lock);
    if(status)
        ERR(status, "lock");

    nSimReps = Stage_getSimReps(self->list);

    status = pthread_mutex_unlock(&self->lock);
    if(status)
        ERR(status, "unlock");

    return nSimReps;
}

long SimSched_getOptItr(SimSched * self) {
    int         status;
    long        nOptItr;

    status = pthread_mutex_lock(&self->lock);
    if(status)
        ERR(status, "lock");

    nOptItr = Stage_getOptItr(self->list);

    status = pthread_mutex_unlock(&self->lock);
    if(status)
        ERR(status, "unlock");

    return nOptItr;
}

// If we're at the last stage already, return 1. Otherwise,
// advance to the next stage and return 0.
int SimSched_next(SimSched * self) {
    int         status;

    if(self->list == NULL)
        return 1;

    status = pthread_mutex_lock(&self->lock);
    if(status)
        ERR(status, "lock");

    self->list = Stage_popHead(self->list);

    status = pthread_mutex_unlock(&self->lock);
    if(status)
        ERR(status, "unlock");

    if(self->list)
        return 1;

    return 0;
}

void        SimSched_print(const SimSched *self, FILE *fp) {
    fprintf(fp, "# %5s %7s %8s\n", "Stage", "nOptItr", "nSimReps");
    Stage *stage;
    int i=0;
    for(stage=self->list; stage != NULL; stage=stage->next) {
        fprintf(fp, "# %5d %7ld %8ld\n",
                i, stage->nOptItr, stage->nSimReps);
        ++i;
    }
}
