#ifndef __THREAD_THREAD_H
#define __THREAD_THREAD_H
#include "stdint.h"
#include "list.h"
#include "memory.h"
typedef void* PVOID;
typedef void VOID;

typedef void THREAD_FUNCRION(void* argc);
typedef void (*PTHREAD_FUNCRION)(void* argc);

typedef enum _TASK_STATUS
{
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED,
    TASK_WAITING,
    TASK_HANGING,
    TASK_DIED
} TASK_STATUS;

typedef struct _INTR_STACK
{
    uint32_t vector_number;
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp_dummy; //ignore this para.
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;

    //used only low RPL into high DPL
    //trapped
    uint32_t error_code;
    void (*eip)(void);
    uint32_t cs;
    uint32_t eflags;
    void *esp;
    uint32_t ss;
} INTR_STACK, *PINTR_STACK;

typedef struct _THREAD_STACK
{
    uint32_t ebp;
    uint32_t ebx;
    uint32_t edi;
    uint32_t esi;
    //eip points to kernel_thread at first time
    //other cases, eip points to switch_to ret address.
    void (*eip)(PTHREAD_FUNCRION func, PVOID func_argc);

    //used for PLACE_HOLDER

    //keep stack balance
    void(*unused_retaddr);
    PTHREAD_FUNCRION function;
    PVOID func_argc;
} THREAD_STACK, *PTHREAD_STACK;

static uint32_t cannary=0x23198408;
//Process Control Block (PCB)
//Task control
typedef struct _TASK_STRUCT
{
    uint32_t *self_kstack;
    TASK_STATUS status;
    uint8_t priority;
    char name[32];
    uint8_t ticks;  //time tick each time on CPU
    uint32_t elapsed_ticks; //ticks since CPU running, e.g. how long task running
    LIST_NODE general_tag;  //general_tag in thread queue
    LIST_NODE all_list_tag; //nodes in thread_all_list 
    uint32_t *pgdir;    //Process Page Table VA
    VISUAL_ADDRESS userprog;
    uint32_t canary;
} TASK_STRUCT, *PTASK_STRUCT;

void thread_create(PTASK_STRUCT pthread,THREAD_FUNCRION func,void* func_argc );
void init_thread_task_structure(PTASK_STRUCT pthread,char* name,int prio);
PTASK_STRUCT thread_start(char* name ,int priority,THREAD_FUNCRION function,void* func_argc);
PTASK_STRUCT get_running_thread();
void schedule(void);
void thread_init(void);
void thread_block(TASK_STATUS stat);
void thread_unblock(PTASK_STRUCT pthread);
#endif
