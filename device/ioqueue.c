#include "ioqueue.h"
#include "interrupt.h"
#include "global.h"
#include "debug.h"

void ioqueue_init(PIOQUEUE ioq)
{
    lock_init(&ioq->lock);
    ioq->producer = ioq->consumer = NULL;
    ioq->head = ioq->tail = 0;
}

//next pos in ioqueue
static int32_t next_pos(int32_t pos)
{
    return (pos + 1) % bufsize;
}

//return true if queue is full
bool ioq_full(PIOQUEUE ioq)
{
    ASSERT(intr_get_status() == INTR_OFF);
    return next_pos(ioq->head) == ioq->tail;
}

//return true if queue is empty
static bool ioq_empty(PIOQUEUE ioq)
{
    //ASSERT(intr_get_status() == INTR_OFF);
    return ioq->head == ioq->tail;
}

//make producer or consumer wait and blocked
static void ioq_wait(PTASK_STRUCT *waiter)
{
    ASSERT(*waiter == NULL && waiter != NULL);
    *waiter = get_running_thread();
    thread_block(TASK_BLOCKED);
}

//wake
static void wakeup(PTASK_STRUCT *waiter)
{
    ASSERT(*waiter != NULL);
    thread_unblock(*waiter);
    *waiter = NULL;
}

//consumer getchar from ioqueue
char ioq_getchar(PIOQUEUE ioq)
{
    ASSERT(intr_get_status() == INTR_OFF);

    //if queue is empty, make consumer wait, e.g. blocked
    while (ioq_empty(ioq))
    {
        lock_acquire(&ioq->lock);
        ioq_wait(&ioq->consumer);
        lock_release(&ioq->lock);
    }

    char byte = ioq->buf[ioq->tail]; // read from queue buf
    ioq->tail = next_pos(ioq->tail); // update pos

    if (ioq->producer != NULL)
    {
        wakeup(&ioq->producer); // wake producer because queue is not full
    }

    return byte;
}

//producer putchar into the queue
void ioq_putchar(PIOQUEUE ioq, char byte)
{
    ASSERT(intr_get_status() == INTR_OFF);

    //if queue is full, make producer wait, e.g. blocked
    while (ioq_full(ioq))
    {
        lock_acquire(&ioq->lock);
        ioq_wait(&ioq->producer);
        lock_release(&ioq->lock);
    }
    ioq->buf[ioq->head] = byte;      
    ioq->head = next_pos(ioq->head); 

    if (ioq->consumer != NULL)
    {
        wakeup(&ioq->consumer);
    }
}
