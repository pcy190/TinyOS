#include "sync.h"
#include "list.h"
#include "global.h"
#include "debug.h"
#include "interrupt.h"
#include "thread.h"

void sema_init(PSEMAPHORE psema, uint8_t value)
{
    psema->value = value;
    list_init(&psema->waiters);
}

void lock_init(PLOCK plock)
{
    plock->holder = NULL;
    plock->holder_repeat_number = 0;
    sema_init(&plock->semaphore, 1); //dual semaphore
}

//down semaphore
void sema_down(PSEMAPHORE psema)
{
    //Atomic operation
    INTR_STATUS old_status = intr_disable();
    while (psema->value == 0)
    {
        //wait to gain
        ASSERT(!elem_find(&psema->waiters, &get_running_thread()->general_tag));
        list_append(&psema->waiters, &get_running_thread()->general_tag);
        thread_block(TASK_BLOCKED);
    }
    psema->value--;
    ASSERT(psema->value == 0);
    intr_set_status(old_status);
}

//up semaphore
void sema_up(PSEMAPHORE psema)
{
    //Atomic operation
    INTR_STATUS old_status = intr_disable();
    if (!list_empty(&psema->waiters))
    {
        //recover thread which wait to gain semaphore
        PTASK_STRUCT pthread = (PTASK_STRUCT)elem2entry(TASK_STRUCT, general_tag,list_pop(&psema->waiters));
        thread_unblock(pthread);
    }
    psema->value++;
    ASSERT(psema->value == 1);
    intr_set_status(old_status);
}
void lock_acquire(PLOCK plock)
{
    if (plock->holder != get_running_thread())
    {
        sema_down(&plock->semaphore);
        plock->holder = get_running_thread();
        ASSERT(plock->holder_repeat_number == 0);
        plock->holder_repeat_number = 1;
    }
    else
    {
        plock->holder_repeat_number++;
    }
}
void lock_release(PLOCK plock){
   ASSERT(plock->holder == get_running_thread());
   if (plock->holder_repeat_number > 1) {
      plock->holder_repeat_number--;
      return;
   }
   ASSERT(plock->holder_repeat_number == 1);

   plock->holder = NULL;	   // avoid schedule makes holder disordered 
   plock->holder_repeat_number = 0;
   sema_up(&plock->semaphore);	   //Atomic operation
}