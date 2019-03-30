#include "thread.h"
#include "stdint.h"
#include "string.h"
#include "memory.h"

#define PG_SIZE 4096

static void kernel_thread(PTHREAD_FUNCRION function, void *func_argc)
{
    function(func_argc);
}

void thread_create(PTASK_STRUCT pthread, THREAD_FUNCRION function, void *argc)
{
    pthread->self_kstack -= sizeof(INTR_STACK);
    pthread->self_kstack -= sizeof(THREAD_STACK);

    PTHREAD_STACK kthread_stack = (PTHREAD_STACK)pthread->self_kstack;
    kthread_stack->eip = kernel_thread;
    kthread_stack->function = function;
    kthread_stack->func_argc = argc;
    kthread_stack->ebp = kthread_stack->ebx = kthread_stack->esi = kthread_stack->edi;
}

void init_thread(PTASK_STRUCT pthread, char *name, int priority)
{
    memset(pthread, 0, sizeof(*pthread));
    strcpy(pthread->name, name); //name len<=32
    pthread->status = TASK_RUNNING;
    pthread->priority = priority;
    pthread->self_kstack = (uint32_t *)((uint32_t)pthread + PG_SIZE);
    pthread->canary = 0x23198408;
}
PTASK_STRUCT thread_start(char *name, int priority, THREAD_FUNCRION function, void *func_argc)
{
    PTASK_STRUCT thread = (PTASK_STRUCT)malloc_kernel_pages(1);
    init_thread(thread, name, priority);
    thread_create(thread, function, func_argc);
    asm volatile("movl %0, %%esp; pop %%ebp; pop %%ebx; pop %%edi; pop %%esi; ret"
                 :
                 : "g"(thread->self_kstack)
                 : "memory");
    return thread;
}