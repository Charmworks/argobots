/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "thread_prioqueue.h"

/** Initialize a Priority Queue */
static void CqsPrioqInit(_prioq *pq)
{
    int i;
    pq->heapsize = 100;
    pq->heapnext = 1;
    pq->hash_key_size = PRIOQ_TABSIZE;
    pq->hash_entry_size = 0;
    pq->heap = (_prioqelt **)malloc(100 * sizeof(_prioqelt *));
    pq->hashtab = (_prioqelt **)malloc(pq->hash_key_size * sizeof(_prioqelt *));
    for (i=0; i<pq->hash_key_size; i++) pq->hashtab[i]=0;
}

#if CMK_C_INLINE
inline
#endif
/** Double the size of a Priority Queue's heap */
static void CqsPrioqExpand(_prioq *pq)
{
    int oldsize = pq->heapsize;
    int newsize = oldsize * 2;
    _prioqelt **oheap = pq->heap;
    _prioqelt **nheap = (_prioqelt **)malloc(newsize*sizeof(_prioqelt*));
    memcpy(nheap, oheap, oldsize * sizeof(_prioqelt *));
    pq->heap = nheap;
    pq->heapsize = newsize;
    free(oheap);
}
/** Double the size of a Priority Queue's hash table */
void CqsPrioqRehash(_prioq *pq)
{
    int oldHsize = pq->hash_key_size;
    int newHsize = oldHsize * 2;
    unsigned int hashval;
    _prioqelt *pe, *pe1, *pe2;
    int i,j;

    _prioqelt **ohashtab = pq->hashtab;
    _prioqelt **nhashtab = (_prioqelt **)malloc(newHsize*sizeof(_prioqelt *));

    pq->hash_key_size = newHsize;

    for(i=0; i<newHsize; i++)
        nhashtab[i] = 0;

    for(i=0; i<oldHsize; i++) {
        for(pe=ohashtab[i]; pe; ) {
            pe2 = pe->ht_next;
            hashval = pe->pri.bits;
            for (j=0; j<pe->pri.ints; j++) hashval ^= pe->pri.data[j];
            hashval = (hashval&0x7FFFFFFF)%newHsize;

            pe1=nhashtab[hashval];
            pe->ht_next = pe1;
            pe->ht_handle = (nhashtab+hashval);
            if (pe1) pe1->ht_handle = &(pe->ht_next);
            nhashtab[hashval]=pe;
            pe = pe2;
        }
    }
    pq->hashtab = nhashtab;
    pq->hash_key_size = newHsize;
    free(ohashtab);
}

int CqsPrioGT_(unsigned int ints1, unsigned int *data1, unsigned int ints2, unsigned int *data2)
{
    unsigned int val1;
    unsigned int val2;
    while (1) {
        if (ints1==0) return 0;
        if (ints2==0) return 1;
        val1 = *data1++;
        val2 = *data2++;
        if (val1 < val2) return 0;
        if (val1 > val2) return 1;
        ints1--;
        ints2--;
    }
}

/**
 * Compare two priorities (treated as unsigned).
 * @return 1 if prio1 > prio2
 * @return ? if prio1 == prio2
 * @return 0 if prio1 < prio2
 */
int CqsPrioGT(_prio *prio1, _prio *prio2)
{
    unsigned int ints1 = prio1->ints;
    unsigned int ints2 = prio2->ints;
    unsigned int *data1 = prio1->data;
    unsigned int *data2 = prio2->data;
    unsigned int val1;
    unsigned int val2;
    while (1) {
        if (ints1==0) return 0;
        if (ints2==0) return 1;
        val1 = *data1++;
        val2 = *data2++;
        if (val1 < val2) return 0;
        if (val1 > val2) return 1;
        ints1--;
        ints2--;
    }
}

/** Find or create a bucket in the hash table for the specified priority. */
thread_queue_t* CqsPrioqGetDeq(_prioq *pq, unsigned int priobits, unsigned int *priodata)
{
    unsigned int prioints = (priobits+CINTBITS-1)/CINTBITS;
    unsigned int hashval, i;
    int heappos; 
    _prioqelt **heap, *pe, *next, *parent;
    _prio *pri;
    int mem_cmp_res;
    unsigned int pri_bits_cmp;
    static int cnt_nilesh=0;

    /* Scan for priority in hash-table, and return it if present */
    hashval = priobits;
    for (i=0; i<prioints; i++) hashval ^= priodata[i];
    hashval = (hashval&0x7FFFFFFF)%PRIOQ_TABSIZE;
    for (pe=pq->hashtab[hashval]; pe; pe=pe->ht_next)
        if (priobits == pe->pri.bits)
            if (memcmp(priodata, pe->pri.data, sizeof(int)*prioints)==0)
                return &(pe->data);

    /* If not present, allocate a bucket for specified priority */
    pe = (_prioqelt *)malloc(sizeof(_prioqelt)+((prioints-1)*sizeof(int)));
    pe->pri.bits = priobits;
    pe->pri.ints = prioints;
    memcpy(pe->pri.data, priodata, (prioints*sizeof(int)));
    //CqsDeqInit(&(pe->data));
    thread_queue_init(&(pe->data));
    pri=&(pe->pri);

    /* Insert bucket into hash-table */
    next = pq->hashtab[hashval];
    pe->ht_next = next;
    pe->ht_handle = (pq->hashtab+hashval);
    if (next) next->ht_handle = &(pe->ht_next);
    pq->hashtab[hashval] = pe;
    pq->hash_entry_size++;
    if(pq->hash_entry_size > 2*pq->hash_key_size)
        CqsPrioqRehash(pq);
    /* Insert bucket into heap */
    heappos = pq->heapnext++;
    if (heappos == pq->heapsize) CqsPrioqExpand(pq);
    heap = pq->heap;
    while (heappos > 1) {
        int parentpos = (heappos >> 1);
        _prioqelt *parent = heap[parentpos];
        if (CqsPrioGT(pri, &(parent->pri))) break;
        heap[heappos] = parent; heappos=parentpos;
    }
    heap[heappos] = pe;

    return &(pe->data);
}

/** Dequeue an entry */
void *CqsPrioqDequeue(_prioq *pq)
{
    _prio *pri;
    _prioqelt *pe, *old;
    //void *data;
    int heappos, heapnext;
    _prioqelt **heap = pq->heap;
    int left_child;
    _prioqelt *temp1_ht_right, *temp1_ht_left, *temp1_ht_parent;
    _prioqelt **temp1_ht_handle;
    static int cnt_nilesh1=0;

    if (pq->heapnext==1) return 0;
    pe = heap[1];
    //data = CqsDeqDequeue(&(pe->data));
    ABTI_thread *data = thread_queue_pop_head(&(pe->data));
    //if (pe->data.head == pe->data.tail) {
    if (pe->data.p_head == pe->data.p_tail) {
        /* Unlink prio-bucket from hash-table */
        _prioqelt *next = pe->ht_next;
        _prioqelt **handle = pe->ht_handle;
        if (next) next->ht_handle = handle;
        *handle = next;
        old=pe;
        pq->hash_entry_size--;

        /* Restore the heap */
        heapnext = (--pq->heapnext);
        pe = heap[heapnext];
        pri = &(pe->pri);
        heappos = 1;
        while (1) {
            int childpos1, childpos2, childpos;
            _prioqelt *ch1, *ch2, *child;
            childpos1 = heappos<<1;
            if (childpos1>=heapnext) break;
            childpos2 = childpos1+1;
            if (childpos2>=heapnext)
            { childpos=childpos1; child=heap[childpos1]; }
            else {
                ch1 = heap[childpos1];
                ch2 = heap[childpos2];
                if (CqsPrioGT(&(ch1->pri), &(ch2->pri)))
                {childpos=childpos2; child=ch2;}
                else {childpos=childpos1; child=ch1;}
            }
            if (CqsPrioGT(&(child->pri), pri)) break;
            heap[heappos]=child; heappos=childpos;
        }
        heap[heappos]=pe;

        /* Free prio-bucket */
        //if (old->data.bgn != old->data.space) free(old->data.bgn);
        free(old);
    }
    return data;
}

static inline void prioq_queue_init(Queue *q)
{
    q->length = 0;
    q->maxlen = 0;
    //CqsDeqInit(&(q->zeroprio));
    thread_queue_init(&(q->zeroprio)); 
    CqsPrioqInit(&(q->negprioq));
    CqsPrioqInit(&(q->posprioq));
}

static inline void prioq_queue_free(Queue *q)
{
    ; /* Do nothing. */
    free(q->negprioq.heap);
    free(q->posprioq.heap);
}





Queue* CqsCreate(void)
{
    Queue *q = (Queue *)malloc(sizeof(Queue));
    q->length = 0;
    q->maxlen = 0;
    //CqsDeqInit(&(q->zeroprio));
    thread_queue_init(&(q->zeroprio)); 
    CqsPrioqInit(&(q->negprioq));
    CqsPrioqInit(&(q->posprioq));
    return q;
}

void CqsDelete(Queue *q)
{
    free(q->negprioq.heap);
    free(q->posprioq.heap);
    free(q);
}

unsigned int CqsMaxLength(Queue *q)
{
    return q->maxlen;
}


unsigned int CqsLength(Queue *q)
{
    return q->length;
}

int CqsEmpty(Queue *q)
{
    return (q->length == 0);
}

static int testEndian(void)
{
    int test=0x1c;
    unsigned char *c=(unsigned char *)&test;
    if (c[sizeof(int)-1]==0x1c)
        /* Macintosh and most workstations are big-endian */
        return 1;   /* Big-endian machine */
    if (c[0]==0x1c)
        /* Intel x86 PC's, and DEC VAX are little-endian */
        return 0;  /* Little-endian machine */
    return -2;  /*Unknown integer type */
}

int CmiEndianness(void)
{
    static int _cmi_endianness = -1;
    if (_cmi_endianness == -1) _cmi_endianness = testEndian();
    assert(_cmi_endianness != -2);
    return  _cmi_endianness;
}



void CqsEnqueueGeneral(Queue *q, ABTI_thread *data, int strategy, 
        int priobits,unsigned int *prioptr)
{
#if CMK_FIFO_QUEUE_ONLY
    //CmiAssert(strategy == CQS_QUEUEING_FIFO);
    assert(strategy == CQS_QUEUEING_FIFO);
    //CqsDeqEnqueueFifo(&(q->zeroprio), data);
    thread_queue_push_tail(&(q->zeroprio), data);
#else
    thread_queue_t *d; int iprio;
    int64_t lprio0, lprio;
    switch (strategy) {
        case CQS_QUEUEING_FIFO: 
            //CqsDeqEnqueueFifo(&(q->zeroprio), data); 
            thread_queue_push_tail(&(q->zeroprio), data);
            break;
        case CQS_QUEUEING_LIFO: 
            //CqsDeqEnqueueLifo(&(q->zeroprio), data); 
            thread_queue_push_head(&(q->zeroprio), data);
            break;
        case CQS_QUEUEING_IFIFO:
            iprio=prioptr[0]+(1U<<(CINTBITS-1));
            if ((int)iprio<0)
                d=CqsPrioqGetDeq(&(q->posprioq), CINTBITS, (unsigned int*)&iprio);
            else d=CqsPrioqGetDeq(&(q->negprioq), CINTBITS, (unsigned int*)&iprio);
            //CqsDeqEnqueueFifo(d, data);
            thread_queue_push_tail(d, data);
            break;
        case CQS_QUEUEING_ILIFO:
            iprio=prioptr[0]+(1U<<(CINTBITS-1));
            if ((int)iprio<0)
                d=CqsPrioqGetDeq(&(q->posprioq), CINTBITS, (unsigned int*)&iprio);
            else d=CqsPrioqGetDeq(&(q->negprioq), CINTBITS, (unsigned int*)&iprio);
            //CqsDeqEnqueueLifo(d, data);
            thread_queue_push_head(d, data);
            break;
        case CQS_QUEUEING_BFIFO:
            if (priobits&&(((int)(prioptr[0]))<0))
                d=CqsPrioqGetDeq(&(q->posprioq), priobits, prioptr);
            else d=CqsPrioqGetDeq(&(q->negprioq), priobits, prioptr);
            //CqsDeqEnqueueFifo(d, data);
            thread_queue_push_tail(d, data);
            break;
        case CQS_QUEUEING_BLIFO:
            if (priobits&&(((int)(prioptr[0]))<0))
                d=CqsPrioqGetDeq(&(q->posprioq), priobits, prioptr);
            else d=CqsPrioqGetDeq(&(q->negprioq), priobits, prioptr);
            //CqsDeqEnqueueLifo(d, data);
            thread_queue_push_head(d, data);
            break;

            /* The following two cases have a 64bit integer as priority value. Therefore,
             * we can cast the address of the int64_t lprio to an "unsigned int" pointer
             * when passing it to CqsPrioqGetDeq. The endianness is taken care explicitly.
             */
        case CQS_QUEUEING_LFIFO:     
            //CmiAssert(priobits == CLONGBITS);
            assert(priobits == CLONGBITS);
            lprio0 =((int64_t *)prioptr)[0];
            lprio0 += (1ULL<<(CLONGBITS-1));
            if (CmiEndianness() == 0) {           /* little-endian */
                lprio =(((uint32_t *)&lprio0)[0]*1LL)<<CINTBITS | ((uint32_t *)&lprio0)[1];
            }
            else {                /* little-endian */
                lprio = lprio0;
            }
            if (lprio0<0)
                d=CqsPrioqGetDeq(&(q->posprioq), priobits, (unsigned int *)&lprio);
            else
                d=CqsPrioqGetDeq(&(q->negprioq), priobits, (unsigned int *)&lprio);
            //CqsDeqEnqueueFifo(d, data);
            thread_queue_push_tail(d, data);
            break;
        case CQS_QUEUEING_LLIFO:
            lprio0 =((int64_t *)prioptr)[0];
            lprio0 += (1ULL<<(CLONGBITS-1));
            if (CmiEndianness() == 0) {           /* little-endian happen to compare least significant part first */
                lprio =(((uint32_t *)&lprio0)[0]*1LL)<<CINTBITS | ((uint32_t *)&lprio0)[1];
            }
            else {                /* little-endian */
                lprio = lprio0;
            }
            if (lprio0<0)
                d=CqsPrioqGetDeq(&(q->posprioq), priobits, (unsigned int *)&lprio);
            else
                d=CqsPrioqGetDeq(&(q->negprioq), priobits, (unsigned int *)&lprio);
            //CqsDeqEnqueueLifo(d, data);
            thread_queue_push_head(d, data);
            break;
        default:
            printf("CqsEnqueueGeneral: invalid queueing strategy.\n");
            //abort();
    }
#endif
    q->length++; if (q->length>q->maxlen) q->maxlen=q->length;
}

void CqsEnqueueFifo(Queue *q, void *data)
{
    //CqsDeqEnqueueFifo(&(q->zeroprio), data);
    thread_queue_push_tail(&(q->zeroprio), data);
    q->length++; if (q->length>q->maxlen) q->maxlen=q->length;
}

void CqsEnqueueLifo(Queue *q, void *data)
{
#if CMK_FIFO_QUEUE_ONLY
    CmiAbort("CMK_FIFO_QUEUE_ONLY is set, but CqsEnqueueLifo was called!\n");
#endif
    //CqsDeqEnqueueLifo(&(q->zeroprio), data);
    thread_queue_push_head(&(q->zeroprio), data);
    q->length++; if (q->length>q->maxlen) q->maxlen=q->length;
}

void CqsEnqueue(Queue *q, void *data)
{
    //CqsDeqEnqueueFifo(&(q->zeroprio), data);
    thread_queue_push_tail(&(q->zeroprio), data);
    q->length++; if (q->length>q->maxlen) q->maxlen=q->length;
}

void CqsDequeue(Queue *q, void **resp)
{
#ifdef ADAPT_SCHED_MEM
    /* Added by Isaac for testing purposes: */
    if((q->length > 1) && (CmiMemoryUsage() > schedAdaptMemThresholdMB*1024*1024) ){
        /* CqsIncreasePriorityForEntryMethod(q, 153); */
        CqsIncreasePriorityForMemCriticalEntries(q); 
    }
#endif

    if (q->length==0) 
    { *resp = 0; return; }
    if (q->negprioq.heapnext>1)
    { *resp = CqsPrioqDequeue(&(q->negprioq)); q->length--; return; }
    //if (q->zeroprio.head != q->zeroprio.tail)
    if (q->zeroprio.p_head != q->zeroprio.p_tail)
    { //*resp = CqsDeqDequeue(&(q->zeroprio)); q->length--; return; }
    *resp = thread_queue_pop_head(&(q->zeroprio)); q->length--; return; }
if (q->posprioq.heapnext>1)
{ *resp = CqsPrioqDequeue(&(q->posprioq)); q->length--; return; }
*resp = 0; return;
}

static _prio kprio_zero = { 0, 0, {0} };
static _prio kprio_max  = { 32, 1, {((unsigned int)(-1))} };

//_prioq kprio_zero = { 0, 0, {0} };
//_prioq kprio_max  = { 32, 1, {((unsigned int)(-1))} };



_prio* CqsGetPriority(Queue *q)
{
    if (q->negprioq.heapnext>1) return &(q->negprioq.heap[1]->pri);
    //if (q->zeroprio.head != q->zeroprio.tail) { return &kprio_zero; }
    if (q->zeroprio.p_head != q->zeroprio.p_tail) { return &kprio_zero; }
    if (q->posprioq.heapnext>1) return &(q->posprioq.heap[1]->pri);
    return &kprio_max;
}


///* prio CqsGetSecondPriority(q) */
///* Queue q; */
///* { */
///*   return CqsGetPriority(q); */
///* } */
//
//
///** Produce an array containing all the entries in a deq
//  @return a newly allocated array filled with copies of the (void*) elements in the deq. 
//  @param [in] q a deq
//  @param [out] num the number of pointers in the returned array
// */
//void** CqsEnumerateDeq(_deq q, int *num){
//    void **head, **tail;
//    void **result;
//    int count = 0;
//    int i;
//
//    head = q->head;
//    tail = q->tail;
//
//    while(head != tail){
//        count++;
//        head++;
//        if(head == q->end)
//            head = q->bgn;
//    }
//
//    result = (void **)malloc(count * sizeof(void *));
//    i = 0;
//    head = q->head;
//    tail = q->tail;
//    while(head != tail){
//        result[i] = *head;
//        i++;
//        head++;
//        if(head == q->end)
//            head = q->bgn;
//    }
//    *num = count;
//    return(result);
//}
//
///** Produce an array containing all the entries in a prioq
//  @return a newly allocated array filled with copies of the (void*) elements in the prioq. 
//  @param [in] q a deq
//  @param [out] num the number of pointers in the returned array
// */
//void** CqsEnumeratePrioq(_prioq q, int *num){
//    void **head, **tail;
//    void **result;
//    int i,j;
//    int count = 0;
//    _prioqelt pe;
//
//    for(i = 1; i < q->heapnext; i++){
//        pe = (q->heap)[i];
//        head = pe->data.head;
//        tail = pe->data.tail;
//        while(head != tail){
//            count++;
//            head++;
//            if(head == (pe->data).end)
//                head = (pe->data).bgn;
//        }
//    }
//
//    result = (void **)malloc((count) * sizeof(void *));
//    *num = count;
//
//    j = 0;
//    for(i = 1; i < q->heapnext; i++){
//        pe = (q->heap)[i];
//        head = pe->data.head;
//        tail = pe->data.tail;
//        while(head != tail){
//            result[j] = *head;
//            j++;
//            head++;
//            if(head ==(pe->data).end)
//                head = (pe->data).bgn; 
//        }
//    }
//
//    return result;
//}
//
//void CqsEnumerateQueue(Queue q, void ***resp){
//    void **result;
//    int num;
//    int i,j;
//
//    *resp = (void **)malloc(q->length * sizeof(void *));
//    j = 0;
//
//    result = CqsEnumeratePrioq(&(q->negprioq), &num);
//    for(i = 0; i < num; i++){
//        (*resp)[j] = result[i];
//        j++;
//    }
//    free(result);
//
//    result = CqsEnumerateDeq(&(q->zeroprio), &num);
//    for(i = 0; i < num; i++){
//        (*resp)[j] = result[i];
//        j++;
//    }
//    free(result);
//
//    result = CqsEnumeratePrioq(&(q->posprioq), &num);
//    for(i = 0; i < num; i++){
//        (*resp)[j] = result[i];
//        j++;
//    }
//    free(result);
//}

/**
  Remove first occurence of a specified entry from the deq  by
  setting the entry to NULL.

  The size of the deq will not change, it will now just contain an
  entry for a NULL pointer.

  @return number of entries that were replaced with NULL
 */
int CqsRemoveSpecificDeq(thread_queue_t *q, const ABTI_thread *specific){
    ABTI_thread *head, *tail;

    head = q->p_head;
    tail = q->p_tail;

    while(head != tail){
        if(head == specific){
            /*    CmiPrintf("Replacing %p in deq with NULL\n", msgPtr); */
            /*     *head = NULL;  */
            return 1;
        }
        head++;
        //if(head == q->end)
        //    head = q->bgn;
    }
    return 0;
}

/**
  Remove first occurence of a specified entry from the prioq by
  setting the entry to NULL.

  The size of the prioq will not change, it will now just contain an
  entry for a NULL pointer.

  @return number of entries that were replaced with NULL
 */
int CqsRemoveSpecificPrioq(_prioq *q, const ABTI_thread *specific){
    ABTI_thread *head, *tail;
    void **result;
    int i;
    _prioqelt *pe;

    for(i = 1; i < q->heapnext; i++){
        pe = (q->heap)[i];
        head = pe->data.p_head;
        tail = pe->data.p_tail;
        while(head != tail){
            if(head == specific){
                /*	CmiPrintf("Replacing %p in prioq with NULL\n", msgPtr); */
                head = NULL;
                return 1;
            }     
            head++;
            //if(head == (pe->data).end)
            //    head = (pe->data).bgn;
        }
    } 
    return 0;
}

void CqsRemoveSpecific(Queue *q, const void *msgPtr){
    if( CqsRemoveSpecificPrioq(&(q->negprioq), msgPtr) == 0 )
        if( CqsRemoveSpecificDeq(&(q->zeroprio), msgPtr) == 0 )  
            if(CqsRemoveSpecificPrioq(&(q->posprioq), msgPtr) == 0){
                //CmiPrintf("Didn't remove the specified entry because it was not found\n");
            }
}
