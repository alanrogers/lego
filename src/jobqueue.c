/**
 * @file jobqueue.c
 * @author Alan R. Rogers
 * @brief Multithreaded job queue
 *
 * This file implements a multithreaded job queue. Jobs are pushed
 * onto a queue by the main program. Each thread (or worker) removes a
 * job from the queue, executes it, and then goes back for
 * another. When all jobs are finished, control returns to the main
 * function.
 *
 * @copyright Copyright (c) 2014, Alan R. Rogers
 * <rogers@anthro.utah.edu>. This file is released under the Internet
 * Systems Consortium License, which can be found in file "LICENSE".
 */

#include "jobqueue.h"
#include "misc.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>

#undef DPRINTF_ON
#include "dprintf.h"
#ifdef DPRINTF_ON
extern pthread_mutex_t outputLock;
#endif

#undef ERR
#define ERR(code, msg) do{                              \
        char err_buff[50];                              \
        strerror_r((code), err_buff, sizeof(err_buff)); \
        fprintf(stderr,"%s:%s:%d: %s: %d (%s)\n",       \
                __FILE__,__func__,__LINE__,             \
                (msg), (code), err_buff);               \
        exit(1);                                        \
    }while(0)

#undef CHECKMEM
#define   CHECKMEM(x) do {                                  \
        if(!(x)) {                                          \
            fprintf(stderr, "%s:%s:%d: allocation error\n", \
                    __FILE__,__func__,__LINE__);            \
            exit(EXIT_FAILURE);                             \
        }                                                   \
    } while(0);

typedef struct Job Job;

/// A single job in the queue
struct Job {
    struct Job *next;           // next job in queue
    void       *param;          // data for current job
    int         (*jobfun) (void *param, void *tdat);    // function that does job
};

/// All data used by job queue
struct JobQueue {

    Job        *todo;           // list of jobs
    bool        acceptingJobs;  // false => don't wait for work
    int         maxThreads;     // maxumum number of threads
    int         nThreads;       // current number of threads
    int         idle;           // number of idle threads
    int         valid;          // has JobQueue been initialized
    pthread_attr_t attr;        // create detached threads
    pthread_mutex_t lock;       // for locking queue
    pthread_cond_t wakeWorker;  // for waking workers
    pthread_cond_t wakeMain;    // for waking main

    // These items allow each thread to construct an object that
    // persists for the life of the thread, is passed to each job
    // processed by that thread, and is destroyed when the thread
    // terminates. This allows the thread to maintain, for example, a
    // random number generator, which is used sequentially by all
    // tasks executed by that thread, but is only allocated and freed
    // once.
    void       *threadData;     // constructor argument; not locally owned
    void       *(*ThreadState_new) (void *threadData);  // constuctor
    void        (*ThreadState_free) (void *threadState);    // destructor
};

#define JOBQUEUE_VALID 8131950

#if 0
pthread_mutex_t stdoutLock = PTHREAD_MUTEX_INITIALIZER;
#endif

void       *threadfun(void *varg);
void        Job_free(Job * job);
#ifdef DPRINTF_ON
void        Job_print(Job * job);

void Job_print(Job * job) {
    if(job == NULL) {
        putchar('\n');
        return;
    }
    printf("%p->", job);
    Job_print(job->next);
}
#endif

/// Construct a JobQueue.
JobQueue   *JobQueue_new(int maxThreads, void *threadData,
                         void *(*ThreadState_new) (void *),
                         void (*ThreadState_free) (void *)) {
    int         i;
    JobQueue   *jq = malloc(sizeof(JobQueue));
    CHECKMEM(jq);

    jq->todo = NULL;
    jq->acceptingJobs = true;
    jq->idle = jq->nThreads = 0;
    jq->maxThreads = maxThreads;
    jq->threadData = threadData;
    jq->ThreadState_new = ThreadState_new;
    jq->ThreadState_free = ThreadState_free;

    // set attr for detached threads
    if((i = pthread_attr_init(&jq->attr)))
        ERR(i, "pthread_attr_init");
    if((i = pthread_attr_setdetachstate(&jq->attr, PTHREAD_CREATE_DETACHED)))
        ERR(i, "pthread_attr_setdetachstate");

    if((i = pthread_mutex_init(&jq->lock, NULL)))
        ERR(i, "pthread_mutex_init");

    if((i = pthread_cond_init(&jq->wakeWorker, NULL)))
        ERR(i, "pthread_cond_init");

    if((i = pthread_cond_init(&jq->wakeMain, NULL)))
        ERR(i, "pthread_cond_init");

    jq->valid = JOBQUEUE_VALID;

    return jq;
}

/// Add a job to the queue.
/// @param jq the JobQueue
/// @param jobfun function to be executed. Takes two void*
/// arguments. The first points to a structure containing data
/// associated with this job. The second points to data associated
/// with the thread that runs the job. For example, the 2nd argument
/// can be used to maintain a separate random number generator within
/// each thread.
/// @param param pointer to structure with parameters for this job
void JobQueue_addJob(JobQueue * jq, int (*jobfun) (void *, void *),
                     void *param) {
    assert(jq);

    int         status;
    pthread_t   id;

    if(jq->valid != JOBQUEUE_VALID) {
        fprintf(stderr, "%s:%s:%d: JobQueue not initialized",
                __FILE__,__func__, __LINE__);
        exit(1);
    }

    if(!jq->acceptingJobs) {
        fprintf(stderr, "%s:%s:%d: JobQueue not accepting jobs\n",
                __FILE__, __func__, __LINE__);
        exit(1);
    }

    Job        *job = malloc(sizeof(Job));
    CHECKMEM(job);
    job->jobfun = jobfun;
    job->param = param;

    status = pthread_mutex_lock(&jq->lock);
    if(status)
        ERR(status, "lock");

    job->next = jq->todo;
    jq->todo = job;

#ifdef DPRINTF_ON
    printf("%s:%s:%d:queue:", __FILE__,__func__, __LINE__);
    Job_print(jq->todo);
#endif

    // If threads are idling, wake one
    if(jq->idle > 0) {

        status = pthread_cond_signal(&jq->wakeWorker);
        if(status)
            ERR(status, "signal wakeWorker");

    } else if(jq->nThreads < jq->maxThreads) {

        // launch a new thread
        DPRINTF(("%s:%s:%d launching thread\n", __FILE__,__func__, __LINE__));
        status = pthread_create(&id, &jq->attr, threadfun, (void *) jq);
        if(status)
            ERR(status, "create");
        ++jq->nThreads;
    }

    status = pthread_mutex_unlock(&jq->lock);
    if(status)
        ERR(status, "unlock");
    else
        DPRINTF(("%s:%s:%d: unlocked\n", __FILE__, __func__, __LINE__));
}

/**
 * Waits until there is a job in the queue, pops it off and executes
 * it, then waits for another.  Runs until jobs are completed and
 * main thread sets acceptingJobs=0.
 */
void       *threadfun(void *arg) {
    DPRINTF(("%s:%d: %s %lu entry\n", __FILE__,__LINE__,
             __func__, (unsigned long) pthread_self()));

    //    struct timespec timeout;
    JobQueue   *jq = (JobQueue *) arg;
    Job        *job;
    int         status;
    void       *threadState = NULL;
    if(jq->ThreadState_new != NULL) {
        threadState = jq->ThreadState_new(jq->threadData);
        CHECKMEM(threadState);
    }

    for(;;) {
        //        clock_gettime(CLOCK_REALTIME, &timeout);
        //        timeout.tv_sec += 3;

        status = pthread_mutex_lock(&jq->lock); // LOCK
        if(status)
            ERR(status, "lock");
        else
            DPRINTF(("%s:%s:%d: locked\n", __FILE__, __func__, __LINE__));

        // Wait while the queue is empty and accepting jobs
        while(NULL == jq->todo && jq->acceptingJobs) {
            DPRINTF(("%s:%s:%d:  awaiting work. todo=%p\n",
                     __FILE__, __func__, __LINE__, jq->todo));

            ++jq->idle;
            if(jq->idle == jq->nThreads) {
                status = pthread_cond_signal(&jq->wakeMain);
                if(status)
                    ERR(status, "signal wakeMain");
            }
            //status = pthread_cond_timedwait(&jq->wakeWorker, &jq->lock,
            //                                &timeout);
            status = pthread_cond_wait(&jq->wakeWorker, &jq->lock);
            --jq->idle;
            //if(status == ETIMEDOUT)
            //    continue;
            if(status)
                ERR(status, "wait wakeWorker");
        }

        /*
         * todo accepting
         *   0     0  <- exit
         *   0     1  <- stay in while loop
         *   1     0  <- do work
         *   1     1  <- do work
         */

        if(NULL == jq->todo) {  // shutting down
            assert(!jq->acceptingJobs);
            break;
        } else {                // do job
            DPRINTF(("%s:%d: %s %lu got work\n", __FILE__,__LINE__,
                     __func__, (unsigned long) pthread_self()));

            // remove job from queue
            assert(NULL != jq->todo);
            job = jq->todo;
            jq->todo = jq->todo->next;

#ifdef DPRINTF_ON
            printf("%s:%s:%d:queue:", __FILE__,__func__, __LINE__);
            Job_print(jq->todo);
#endif

            status = pthread_mutex_unlock(&jq->lock);   // UNLOCK
            if(status)
                ERR(status, "unlock");
            else
                DPRINTF(("%s:%s:%d: unlocked\n", __FILE__, __func__,
                         __LINE__));

            DPRINTF(("%s:%d: %s %lu calling jobfun\n", __FILE__,__LINE__,
                     __func__, (unsigned long) pthread_self()));
            job->jobfun(job->param, threadState);
            DPRINTF(("%s:%d: %s %lu back fr jobfun\n", __FILE__, __LINE__,
                     __func__, (unsigned long) pthread_self()));
            free(job);
        }
    }
    // still have lock
    --jq->nThreads;

    status = pthread_cond_signal(&jq->wakeMain);
    if(status)
        ERR(status, "signal wakeMain");

    status = pthread_mutex_unlock(&jq->lock);   // UNLOCK
    if(status)
        ERR(status, "unlock");
    else
        DPRINTF(("%s:%s:%d: unlocked\n", __FILE__, __func__, __LINE__));

    if(threadState)
        jq->ThreadState_free(threadState);

    DPRINTF(("%s:%d: %s %lu exit\n", __FILE__,__LINE__,
             __func__, (unsigned long) pthread_self()));
    return NULL;
}

/// Stop accepting jobs
void JobQueue_noMoreJobs(JobQueue * jq) {
    int         status;

    DPRINTF(("%s:%s:%d: entry\n", __FILE__,__func__, __LINE__));

    if(jq->valid != JOBQUEUE_VALID) {
        fprintf(stderr, "%s:%s:%d: JobQueue not initialized",
                __FILE__, __func__, __LINE__);
        exit(1);
    }

    status = pthread_mutex_lock(&jq->lock);
    if(status)
        ERR(status, "lock");
    else
        DPRINTF(("%s:%s:%d: locked\n", __FILE__, __func__, __LINE__));

    jq->acceptingJobs = false;

    if(jq->idle > 0) {
        // Wake workers so they can quit
        DPRINTF(("%s:%s:%d: telling workers to quit\n",
                 __FILE__,__func__, __LINE__));
        status = pthread_cond_broadcast(&jq->wakeWorker);
        if(status)
            ERR(status, "broadcast wakeWorker");
    }

    status = pthread_mutex_unlock(&jq->lock);
    if(status)
        ERR(status, "unlock");
    else
        DPRINTF(("%s:%s:%d: unlocked\n", __FILE__, __func__, __LINE__));

    DPRINTF(("%s:%s:%d: exit\n", __FILE__,__func__, __LINE__));
}

/// Wait until all threads are idle
void JobQueue_waitOnJobs(JobQueue * jq) {
    int         status;

    if(jq->valid != JOBQUEUE_VALID) {
        fprintf(stderr, "%s:%s:%d: JobQueue not initialized",
                __FILE__, __func__, __LINE__);
        exit(1);
    }

    status = pthread_mutex_lock(&jq->lock);
    if(status)
        ERR(status, "lock");
    else
        DPRINTF(("%s:%s:%d: locked\n", __FILE__, __func__, __LINE__));

    // Wait until jobs are finished.
    while(jq->todo != NULL || jq->idle < jq->nThreads) {

        // If any workers are idle, wake one
        if(jq->idle > 0) {
            status = pthread_cond_signal(&jq->wakeWorker);
            if(status)
                ERR(status, "signal wakeWorker");
        }

        status = pthread_cond_wait(&jq->wakeMain, &jq->lock);
        if(status)
            ERR(status, "wait wakeMain");
    }

    assert(jq->todo == NULL && jq->idle == jq->nThreads);
    DPRINTF(("%s:%s:%d: queue is empty and all threads are idle\n",
             __FILE__, __func__, __LINE__));

    if(!jq->acceptingJobs) {
        // We're done: wake all workers so they can quit
        DPRINTF(("%s:%s:%d: telling workers to quit\n",
                 __FILE__, __func__, __LINE__));
        status = pthread_cond_broadcast(&jq->wakeWorker);
        if(status)
            ERR(status, "broadcast wakeWorker");
    }

    status = pthread_mutex_unlock(&jq->lock);
    if(status)
        ERR(status, "unlock");
    else
        DPRINTF(("%s:%s:%d: unlocked\n", __FILE__, __func__, __LINE__));
}

/// Destroy a Job
void Job_free(Job * job) {
    if(NULL == job)
        return;
    Job_free(job->next);
    free(job);
}

/// Destroy a JobQueue
void JobQueue_free(JobQueue * jq) {
    assert(jq);

    if(jq->valid != JOBQUEUE_VALID) {
        fprintf(stderr, "%s:%s:%d: JobQueue not initialized",
                __FILE__,__func__, __LINE__);
        exit(1);
    }

    int         status;

    JobQueue_noMoreJobs(jq);
    JobQueue_waitOnJobs(jq);

    // sleep to give threads time to release mutex
    struct timespec t = {
        .tv_sec = 0,
        .tv_nsec = 100000000L    // 1/10 of a second
    };
    status = nanosleep(&t, NULL);
    if(status)
        ERR(status, "nanosleep");

    status = pthread_attr_destroy(&jq->attr);
    if(status)
        ERR(status, "destroy attr");

    status = pthread_mutex_destroy(&jq->lock);
    if(status) {
#ifndef NDEBUG
		dostacktrace(__FILE__,__LINE__, stderr);
#endif
        ERR(status, "destroy lock");
	}

    status = pthread_cond_destroy(&jq->wakeWorker);
    if(status)
        ERR(status, "destroy wakeWorker");

    status = pthread_cond_destroy(&jq->wakeMain);
    if(status)
        ERR(status, "destroy wakeMain");

    Job_free(jq->todo);
    free(jq);
}
