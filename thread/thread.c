#include "thread.h"
#include "stdint.h"
#include "string.h"
#include "memory.h"
#include "interrupt.h"
#include "debug.h"
#include "print.h"
#include "list.h"

#define PG_SIZE 4096
extern uint32_t cannary;
PTASK_STRUCT main_thread; //main thread PCB  (FROM KERNEL)
LIST thread_ready_list;
LIST thread_all_list;
static PLIST_NODE thread_tag; //thread node in queue

extern void switch_to(PTASK_STRUCT cur, PTASK_STRUCT next);

//get current thread PCB pointer
PTASK_STRUCT get_running_thread()
{
    uint32_t esp;
    asm("mov %%esp, %0"
        : "=g"(esp));
    //PCB start at Page Base
    return (PTASK_STRUCT)(esp & 0xFFFFF000);
}

static void kernel_thread(PTHREAD_FUNCRION function, void *func_argc)
{
    intr_enable();
    function(func_argc);
}

//set TASK_STRUCT ready
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

//set name and priority.
//init canary and stack point;
void init_thread_task_structure(PTASK_STRUCT pthread, char *name, int priority)
{
    memset(pthread, 0, sizeof(*pthread));
    strcpy(pthread->name, name); //name len<=32
    if (pthread == main_thread)
    {
        /* main is also a thread, its status is always TASK_RUNNING */
        pthread->status = TASK_RUNNING;
    }
    else
    {
        pthread->status = TASK_READY;
    }
    pthread->priority = priority;
    pthread->self_kstack = (uint32_t *)((uint32_t)pthread + PG_SIZE);
    pthread->ticks = priority;
    pthread->elapsed_ticks = 0;
    pthread->pgdir = NULL;
    pthread->canary = cannary;
}

//create a thread and make it ready to run
PTASK_STRUCT thread_start(char *name, int priority, THREAD_FUNCRION function, void *func_argc)
{
    PTASK_STRUCT thread = (PTASK_STRUCT)malloc_kernel_pages(1);
    init_thread_task_structure(thread, name, priority);
    thread_create(thread, function, func_argc);
    /*asm volatile("movl %0, %%esp; pop %%ebp; pop %%ebx; pop %%edi; pop %%esi; ret"
                 :
                 : "g"(thread->self_kstack)
                 : "memory");*/
    // ensure not in queue
    ASSERT(!elem_find(&thread_ready_list, &thread->general_tag));
    //add in ready thread list
    list_append(&thread_ready_list, &thread->general_tag);

    // ensure not in queue
    ASSERT(!elem_find(&thread_all_list, &thread->all_list_tag));
    //add in all thread list
    list_append(&thread_all_list, &thread->all_list_tag);
    return thread;
}

static void make_kernel_main_thread(void)
{
    //kernel PCB reserved at 0xc009e00      //defined in loader.S
    //so we needn't to malloc a page
    main_thread = get_running_thread();
    init_thread_task_structure(main_thread, "main", 31);

    ASSERT(!elem_find(&thread_all_list, &main_thread->all_list_tag));

    list_append(&thread_all_list, &main_thread->all_list_tag);
}

void schedule()
{
    ASSERT(intr_get_status() == INTR_OFF);

    PTASK_STRUCT cur = get_running_thread();
    if (cur->status == TASK_RUNNING) // thread only timeout
    {
        ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
        list_append(&thread_ready_list, &cur->general_tag);
        cur->ticks = cur->priority;
        cur->status = TASK_READY;
    }
    else
    {
        //other reason
    }
    ASSERT(!list_empty(&thread_ready_list));
    //schedule next thread task in queue to run
    //current running thread isn't in task queue
    thread_tag = list_pop(&thread_ready_list);
    PTASK_STRUCT next = elem2entry(TASK_STRUCT, general_tag, thread_tag);
    next->status = TASK_RUNNING;
    switch_to(cur, next);
}

void thread_init()
{
    put_str("thread_init start\n");
    // uint32_t res;
    // for (int i = 0; i < 4; i++)
    // {
    //     asm("movb 0x3,%%ax ;
    //     movb 0x41,%%dx  ;
    //     out %%ax ,%%dx ;
    //     in %%dx,%0;"
    //         : "=ba"(res));
    //     res =(res<< 4);
    // }
    // cannary=res;
    put_str("Cannary is:");
    put_int(cannary);
    put_char('\n');
    list_init(&thread_ready_list);
    list_init(&thread_all_list);
    make_kernel_main_thread();
    put_str("thread_init done\n");
}
