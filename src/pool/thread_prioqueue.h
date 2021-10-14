/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef THREAD_PRIOQUEUE_H_INCLUDED
#define THREAD_PRIOQUEUE_H_INCLUDED

#include "abti.h"
#include "thread_queue.h"

#define CQS_QUEUEING_FIFO 2
#define CQS_QUEUEING_LIFO 3
#define CQS_QUEUEING_IFIFO 4
#define CQS_QUEUEING_ILIFO 5
#define CQS_QUEUEING_BFIFO 6
#define CQS_QUEUEING_BLIFO 7
#define CQS_QUEUEING_LFIFO 8
#define CQS_QUEUEING_LLIFO 9


/* Structure to store a variable bit length priority */
typedef struct prio_struct {
    unsigned short bits;
    unsigned short ints;
    unsigned int data[1];
} *_prio;

typedef struct prioqelt_struct {
    thread_queue_t data;
    struct prioqelt_struct *ht_next;
    struct prioqelt_struct **ht_handle;
    struct prio_struct pri;
}*_prioqelt;

typedef struct {
    int heapsize;
    int heapnext;
    _prioqelt *heap;
    _prioqelt *hashtab;
    int hash_key_size;
    int hash_entry_size;
} _prioq;

typedef struct {
    unsigned int length;
    unsigned int maxlen;
    thread_queue_t zeroprio;
    _prioq negprioq;
    _prioq posprioq;
} Queue;

#define PRIOQ_TABSIZE 1017

#ifndef CINTBITS
#define CINTBITS ((unsigned int) (sizeof(int)*8))
#endif
#ifndef CLONGBITS
#define CLONGBITS ((unsigned int) (sizeof(int64_t)*8))
#endif

#endif /* THREAD_PRIOQUEUE_H_INCLUDED */
