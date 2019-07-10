#ifndef __THREAD_THREAD_H
#define __THREAD_THREAD_H
#include "bitmap.h"
#include "list.h"
#include "memory.h"
#include "stdint.h"

#define MAX_FILES_OPEN_PER_PROC 8

// typed function
typedef void thread_func( void* );
typedef void THREAD_FUNCRION( void* argc );
typedef void ( *PTHREAD_FUNCRION )( void* argc );
typedef int16_t pid_t;

// status of thread/process
enum task_status { TASK_RUNNING, TASK_READY, TASK_BLOCKED, TASK_WAITING, TASK_HANGING, TASK_DIED };
typedef enum task_status TASK_STATUS;
/***********   interrupt stack   ***********
 *  interrupt stack on the top of the process page memory.
 ********************************************/
typedef struct _INTR_STACK {
    uint32_t vec_no;  // kernel.S used in VECTOR push %1 interrupt number
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp_dummy;  // esp placeholder. because esp is always changed.
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;

    // used when CPU from low privilege level to high privilege level
    uint32_t err_code;
    void ( *eip )( void );
    uint32_t cs;
    uint32_t eflags;
    void* esp;
    uint32_t ss;
} INTR_STACK, *PINTR_STACK;

/***********  thread_stack  ***********
 * used when thread encounter switch_to func
 ******************************************/
typedef struct _THREAD_STACK {
    uint32_t ebp;
    uint32_t ebx;
    uint32_t edi;
    uint32_t esi;

    // at first time, eip points to kernel_thread
    // then, eip points to switch_to ret address
    void ( *eip )( thread_func* func, void* func_arg );

    /*****   USED ONLY SCHEDULE TO CPU AT FIRST TIME   ****/

    void( *unused_retaddr );  // ret addr placeholder
    thread_func* function;    // function name
    void* func_arg;           // function arguments
} THREAD_STACK, *PTHREAD_STACK;

// process control block. PCB
typedef struct _TASK_STRUCT {
    uint32_t* self_kstack;  // self kernel stack
    pid_t pid;
    enum task_status status;
    char name[ 16 ];
    uint8_t priority;
    uint8_t ticks;

    // executed ticks ( i.e. running time )
    uint32_t elapsed_ticks;

    int32_t fd_table[ MAX_FILES_OPEN_PER_PROC ];  // file describe table

    // used for most list's node tag
    LIST_NODE general_tag;

    // used for thread_all_list's node tag
    LIST_NODE all_list_tag;

    uint32_t* pgdir;  // page va

    VIRTUAL_ADDRESS userprog_vaddr;  // process va
    MEM_BLOCK_DESC u_block_desc[ DESC_CNT ];
    uint32_t cwd_inode_number;
    int16_t parent_pid;  // parent process pid
    uint32_t canary;     // canary
} TASK_STRUCT, *PTASK_STRUCT;

extern LIST thread_ready_list;
extern LIST thread_all_list;

void thread_create( PTASK_STRUCT pthread, thread_func function, void* func_arg );
void init_thread( PTASK_STRUCT pthread, char* name, int prio );
PTASK_STRUCT thread_start( char* name, int prio, thread_func function, void* func_arg );
PTASK_STRUCT running_thread( void );
PTASK_STRUCT get_running_thread( void );
PTASK_STRUCT running_thread( void );
void schedule( void );
void thread_init( void );
void thread_block( enum task_status stat );
void thread_unblock( PTASK_STRUCT pthread );
void thread_yield( void );
pid_t fork_pid( void );
#endif
