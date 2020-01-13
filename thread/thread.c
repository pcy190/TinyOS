#include "thread.h"
#include "debug.h"
#include "file.h"
#include "fs.h"
#include "global.h"
#include "interrupt.h"
#include "list.h"
#include "memory.h"
#include "print.h"
#include "process.h"
#include "stdint.h"
#include "string.h"
#include "sync.h"

#include "console.h"

// extern uint32_t cannary;
PTASK_STRUCT main_thread;  // main thread PCB  (FROM KERNEL)
PTASK_STRUCT idle_thread;  // idle thread
LIST thread_ready_list;
LIST thread_all_list;
LOCK pid_lock;                 // pid lock
static PLIST_NODE thread_tag;  // thread node in queue

extern void switch_to( PTASK_STRUCT cur, PTASK_STRUCT next );
extern void init( void );

// idle thread
static void idle( void* arg UNUSED ) {
    while ( 1 ) {
        thread_block( TASK_BLOCKED );
        // ensure intr open before hlt
        asm volatile( "sti; hlt" : : : "memory" );
    }
}

// get current thread PCB pointer
PTASK_STRUCT get_running_thread() {
    uint32_t esp;
    asm( "mov %%esp, %0" : "=g"( esp ) );
    // PCB start at Page Base
    return ( PTASK_STRUCT )( esp & 0xfffff000 );
}
PTASK_STRUCT running_thread() {
    uint32_t esp;
    asm( "mov %%esp, %0" : "=g"( esp ) );
    // PCB start at Page Base
    return ( PTASK_STRUCT )( esp & 0xfffff000 );
}

static void kernel_thread( PTHREAD_FUNCRION function, void* func_argc ) {
    intr_enable();
    function( func_argc );
}

// allocate_pid
static pid_t allocate_pid() {
    static pid_t next_pid = 0;
    lock_acquire( &pid_lock );
    next_pid++;
    lock_release( &pid_lock );
    return next_pid;
}

pid_t fork_pid( void ) { return allocate_pid(); }

// set TASK_STRUCT ready
void thread_create( PTASK_STRUCT pthread, THREAD_FUNCRION function, void* argc ) {
    pthread->self_kstack -= sizeof( INTR_STACK );
    pthread->self_kstack -= sizeof( THREAD_STACK );

    PTHREAD_STACK kthread_stack = ( PTHREAD_STACK )pthread->self_kstack;
    kthread_stack->eip = kernel_thread;
    kthread_stack->function = function;
    kthread_stack->func_arg = argc;
    kthread_stack->ebp = kthread_stack->ebx = kthread_stack->esi = kthread_stack->edi = 0;
}

// set name and priority.
// init canary and stack point;
void init_thread( PTASK_STRUCT pthread, char* name, int priority ) {
    memset( pthread, 0, sizeof( *pthread ) );
    pthread->pid = allocate_pid();
    strcpy( pthread->name, name );  // name len<=32
    if ( pthread == main_thread ) {
        /* main is also a thread, its status is always TASK_RUNNING */
        pthread->status = TASK_RUNNING;
    } else {
        pthread->status = TASK_READY;
    }
    pthread->priority = priority;
    pthread->self_kstack = ( uint32_t* )( ( uint32_t )pthread + PG_SIZE );
    pthread->ticks = priority;
    pthread->elapsed_ticks = 0;
    pthread->pgdir = NULL;

    // reserve FD for stdin stdout stderr
    pthread->fd_table[ 0 ] = 0;
    pthread->fd_table[ 1 ] = 1;
    pthread->fd_table[ 2 ] = 2;
    // other fd set as -1
    uint8_t fd_idx = 3;
    while ( fd_idx < MAX_FILES_OPEN_PER_PROC ) {
        pthread->fd_table[ fd_idx ] = -1;
        fd_idx++;
    }
    pthread->cwd_inode_number = 0;
    pthread->parent_pid = -1;
    // todo --canary!
    pthread->canary = 0x19870916;
    // pthread->canary = cannary;
}

// create a thread and make it ready to run
PTASK_STRUCT thread_start( char* name, int priority, THREAD_FUNCRION function, void* func_argc ) {
    PTASK_STRUCT thread = ( PTASK_STRUCT )get_kernel_pages( 1 );
    init_thread( thread, name, priority );
    thread_create( thread, function, func_argc );
    /*asm volatile("movl %0, %%esp; pop %%ebp; pop %%ebx; pop %%edi; pop %%esi;
       ret"
                 :
                 : "g"(thread->self_kstack)
                 : "memory");*/
    // ensure not in queue
    ASSERT( !elem_find( &thread_ready_list, &thread->general_tag ) );
    // add to ready thread list
    list_append( &thread_ready_list, &thread->general_tag );
    // ensure not in queue
    ASSERT( !elem_find( &thread_all_list, &thread->all_list_tag ) );
    // add in all thread list
    list_append( &thread_all_list, &thread->all_list_tag );
    return thread;
}

static void make_kernel_main_thread( void ) {
    // kernel PCB reserved at 0xc009e00      //defined in loader.S
    // so we needn't to malloc a page
    main_thread = get_running_thread();
    init_thread( main_thread, "main", 31 );

    ASSERT( !elem_find( &thread_all_list, &main_thread->all_list_tag ) );

    list_append( &thread_all_list, &main_thread->all_list_tag );
}

void schedule() {
    ASSERT( intr_get_status() == INTR_OFF );

    PTASK_STRUCT cur = get_running_thread();
    if ( cur->status == TASK_RUNNING )  // thread only timeout
    {
        ASSERT( !elem_find( &thread_ready_list, &cur->general_tag ) );
        list_append( &thread_ready_list, &cur->general_tag );
        cur->ticks = cur->priority;
        cur->status = TASK_READY;
    } else {
        // other reason
    }

    // if no running thread, then unblock idle
    if ( list_empty( &thread_ready_list ) ) {
        thread_unblock( idle_thread );
    }

    ASSERT( !list_empty( &thread_ready_list ) );
    // schedule next thread task in queue to run
    // current running thread isn't in task queue
    // thread_tag = list_pop(&thread_ready_list);
    thread_tag = NULL;
    thread_tag = list_pop( &thread_ready_list );

    PTASK_STRUCT next = elem2entry( TASK_STRUCT, general_tag, thread_tag );
    next->status = TASK_RUNNING;
    process_activate( next );
    switch_to( cur, next );
}

// block cur thread and set status as stat
void thread_block( TASK_STATUS stat ) {
    ASSERT( ( ( stat == TASK_BLOCKED ) || ( stat == TASK_WAITING ) || ( stat == TASK_HANGING ) ) );
    INTR_STATUS old_status = intr_disable();
    PTASK_STRUCT cur_thread = get_running_thread();
    cur_thread->status = stat;
    schedule();
    intr_set_status( old_status );
}

// unblock the thread
void thread_unblock( PTASK_STRUCT pthread ) {
    INTR_STATUS old_status = intr_disable();
    ASSERT( ( ( pthread->status == TASK_BLOCKED ) || ( pthread->status == TASK_WAITING ) || ( pthread->status == TASK_HANGING ) ) );
    if ( pthread->status != TASK_READY ) {
        ASSERT( !elem_find( &thread_ready_list, &pthread->general_tag ) );
        if ( elem_find( &thread_ready_list, &pthread->general_tag ) ) {
            PANIC( "thread_unblock: blocked thread in ready_list\n" );
        }
        list_push( &thread_ready_list, &pthread->general_tag );
        pthread->status = TASK_READY;
    }
    intr_set_status( old_status );
}

// yield thread, release CPU and switch to other thread
void thread_yield( void ) {
    PTASK_STRUCT cur = running_thread();
    INTR_STATUS old_status = intr_disable();
    ASSERT( !elem_find( &thread_ready_list, &cur->general_tag ) );
    list_append( &thread_ready_list, &cur->general_tag );
    cur->status = TASK_READY;
    schedule();
    intr_set_status( old_status );
}

// print with align blank space
static void pad_print( char* buf, int32_t buf_len, void* ptr, char format ) {
    memset( buf, 0, buf_len );
    uint8_t out_pad_0idx = 0;
    switch ( format ) {
    case 's':
        out_pad_0idx = sprintf( buf, "%s", ptr );
        break;
    case 'd':
        out_pad_0idx = sprintf( buf, "%d", *( ( int16_t* )ptr ) );
    case 'x':
        out_pad_0idx = sprintf( buf, "%x", *( ( uint32_t* )ptr ) );
    }
    while ( out_pad_0idx < buf_len ) {  // padding with blank space
        buf[ out_pad_0idx ] = ' ';
        out_pad_0idx++;
    }
    sys_write( FD_STDOUT, buf, buf_len - 1 );
}

// print thread info in list_traversal
static bool elem2thread_info( struct list_elem* pelem, int arg UNUSED ) {
    PTASK_STRUCT pthread = elem2entry( TASK_STRUCT, all_list_tag, pelem );
    char out_pad[ 16 ] = {0};

    pad_print( out_pad, 16, &pthread->pid, 'd' );

    if ( pthread->parent_pid == -1 ) {
        pad_print( out_pad, 16, "NULL", 's' );
    } else {
        pad_print( out_pad, 16, &pthread->parent_pid, 'd' );
    }

    switch ( pthread->status ) {
    case TASK_RUNNING:
        pad_print( out_pad, 16, "RUNNING", 's' );
        break;
    case TASK_READY:
        pad_print( out_pad, 16, "READY", 's' );
        break;
    case TASK_BLOCKED:
        pad_print( out_pad, 16, "BLOCKED", 's' );
        break;
    case TASK_WAITING:
        pad_print( out_pad, 16, "WAITING", 's' );
        break;
    case TASK_HANGING:
        pad_print( out_pad, 16, "HANGING", 's' );
        break;
    case TASK_DIED:
        pad_print( out_pad, 16, "DIED", 's' );
    }
    pad_print( out_pad, 16, &pthread->elapsed_ticks, 'x' );

    memset( out_pad, 0, 16 );
    ASSERT( strlen( pthread->name ) < 17 );
    memcpy( out_pad, pthread->name, strlen( pthread->name ) );
    strcat( out_pad, "\n" );
    sys_write( FD_STDOUT, out_pad, strlen( out_pad ) );
    return false;  // continue traversal
}

// print task list
void sys_ps( void ) {
    char* ps_title = "PID            PPID           STAT           TICKS          COMMAND\n";
    sys_write( FD_STDOUT, ps_title, strlen( ps_title ) );
    list_traversal( &thread_all_list, elem2thread_info, 0 );
}

void thread_init() {
    put_str( "thread_init start\n" );
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
    // TODO : check canary
    // put_str("Cannary is:");
    // put_int(cannary);
    // put_char('\n');
    list_init( &thread_ready_list );
    list_init( &thread_all_list );
    lock_init( &pid_lock );
    process_execute( init, "init" );  // init process, pid=1
    make_kernel_main_thread();
    idle_thread = thread_start( "idle", 10, idle, NULL );
    put_str( "thread_init done\n" );
}
