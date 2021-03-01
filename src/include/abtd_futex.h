/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTD_FUTEX_H_INCLUDED
#define ABTD_FUTEX_H_INCLUDED

#ifndef ABT_CONFIG_ACTIVE_WAIT_POLICY

/* ABTD_futex_multiple supports a wait-broadcast pattern.  ABTD_futex_multiple
 * allows multiple waiters. */
typedef struct ABTD_futex_multiple ABTD_futex_multiple;

/* Initialize ABTD_futex_multiple. */
static inline void ABTD_futex_multiple_init(ABTD_futex_multiple *p_futex);

/* This routine unlocks p_lock and makes the underlying Pthread block on
 * p_futex.  Broadcast can wake up this waiter.  Spurious wakeup does not
 * happen. */
void ABTD_futex_wait_and_unlock(ABTD_futex_multiple *p_futex,
                                ABTD_spinlock *p_lock);

/* This routine unlocks p_lock and makes the underlying Pthread block on
 * p_futex.  Broadcast can wake up this waiter.  By nature, spurious wakeup
 * might happen, so the caller needs to check the current time if necessary. */
void ABTD_futex_timedwait_and_unlock(ABTD_futex_multiple *p_futex,
                                     ABTD_spinlock *p_lock,
                                     double wait_time_sec);

/* This routine wakes up waiters that are waiting on p_futex.  This function
 * must be called when a lock (p_lock above) is taken. */
void ABTD_futex_broadcast(ABTD_futex_multiple *p_futex);

#ifdef ABT_CONFIG_USE_LINUX_FUTEX

struct ABTD_futex_multiple {
    ABTD_atomic_int val;
};

static inline void ABTD_futex_multiple_init(ABTD_futex_multiple *p_futex)
{
    ABTD_atomic_relaxed_store_int(&p_futex->val, 0);
}

#else /* ABT_CONFIG_USE_LINUX_FUTEX */

struct ABTD_futex_multiple {
    void *p_next; /* pthread_sync */
};

static inline void ABTD_futex_multiple_init(ABTD_futex_multiple *p_futex)
{
    p_futex->p_next = NULL;
}

#endif /* !ABT_CONFIG_USE_LINUX_FUTEX */

#endif /* !ABT_CONFIG_ACTIVE_WAIT_POLICY */

#endif /* ABTD_FUTEX_H_INCLUDED */
