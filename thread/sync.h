#ifndef __THREAD_SYNC_H
#define __THREAD_SYNC_H
#include "list.h"
#include "stdint.h"
#include "thread.h"

typedef struct _SEMAPHORE
{
    uint8_t value;
    LIST waiters;
} SEMAPHORE, *PSEMAPHORE;

typedef struct _LOCK
{
    PTASK_STRUCT holder;        // lock holder
    SEMAPHORE semaphore;        
    uint32_t holder_repeat_number;  // holder query repeat time
}LOCK,*PLOCK;

void sema_init(PSEMAPHORE psema, uint8_t value);
void sema_down(PSEMAPHORE psema);
void sema_up(PSEMAPHORE psema);
void lock_init(PLOCK plock);
void lock_acquire(PLOCK plock);
void lock_release(PLOCK plock);
#endif
