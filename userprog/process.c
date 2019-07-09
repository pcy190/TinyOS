#include "process.h"
#include "console.h"
#include "debug.h"
#include "global.h"
#include "interrupt.h"
#include "list.h"
#include "memory.h"
#include "string.h"
#include "thread.h"
#include "tss.h"

extern void intr_exit( void );

// init user process stack struct
void start_process( void* filename_ ) {
    void* function = filename_;
    PTASK_STRUCT cur = get_running_thread();
    cur->self_kstack += sizeof( THREAD_STACK );
    PINTR_STACK proc_stack = ( PINTR_STACK )cur->self_kstack;
    proc_stack->edi = proc_stack->esi = proc_stack->ebp = proc_stack->esp_dummy = 0;
    proc_stack->ebx = proc_stack->edx = proc_stack->ecx = proc_stack->eax = 0;
    proc_stack->gs = 0;  // set 0, not used in user mode
    proc_stack->ds = proc_stack->es = proc_stack->fs = SELECTOR_U_DATA;
    proc_stack->eip = function;  // function address to exec
    proc_stack->cs = SELECTOR_U_CODE;
    proc_stack->eflags = ( EFLAGS_IOPL_0 | EFLAGS_MBS | EFLAGS_IF_1 );
    proc_stack->esp = ( void* )( ( uint32_t )get_a_page( PF_USER, USER_STACK3_VADDR ) + PG_SIZE );
    proc_stack->ss = SELECTOR_U_DATA;
    asm volatile( "movl %0, %%esp; jmp intr_exit" : : "g"( proc_stack ) : "memory" );
}

// activate page
void page_dir_activate( PTASK_STRUCT p_thread ) {
    /********************************************************
     * activate page for both thread and process
     * activate thread page to avoid use the last process's page
     ********************************************************/

    uint32_t pagedir_phy_addr = 0x100000;  // default kernel page dir table
    if ( p_thread->pgdir != NULL ) {       // user process's pgdir
        pagedir_phy_addr = addr_v2p( ( uint32_t )p_thread->pgdir );
    }

    // refresh cr3 to activate page
    asm volatile( "movl %0, %%cr3" : : "r"( pagedir_phy_addr ) : "memory" );
}

// activate page dir and update tss's esp0 as process privilege level 0 stack
void process_activate( PTASK_STRUCT p_thread ) {
    ASSERT( p_thread != NULL );
    page_dir_activate( p_thread );

    // update esp0 only for user process
    // kernel process is no need to change to privilege level 0 stack
    if ( p_thread->pgdir ) {
        update_tss_esp( p_thread );
    }
}

// create create_page_dir and copy kernel pde.
// return page dir va if success
// return -1 if fail
uint32_t* create_page_dir( void ) {

    // page dir shouldn't be accessed to user process.
    // create in kernel memory
    uint32_t* page_dir_vaddr = get_kernel_pages( 1 );
    if ( page_dir_vaddr == NULL ) {
        console_put_str( "create_page_dir: get_kernel_page failed!" );
        return NULL;
    }

    /************************** 1  copy page dir  *************************************/
    /*  page_dir_vaddr + 0x300*4 --------> 768th in kernel page dir table */
    memcpy( ( uint32_t* )( ( uint32_t )page_dir_vaddr + 0x300 * 4 ), ( uint32_t* )( 0xfffff000 + 0x300 * 4 ), 1024 );
    /*****************************************************************************/

    /************************** 2  udpate page dir **********************************/
    uint32_t new_page_dir_phy_addr = addr_v2p( ( uint32_t )page_dir_vaddr );
    // page dir address store in the last entry of table.
    // update dir table address as new page dir physical address
    page_dir_vaddr[ 1023 ] = new_page_dir_phy_addr | PG_US_U | PG_RW_W | PG_P_1;
    /*****************************************************************************/
    return page_dir_vaddr;
}

// create user proces va bitmap
void create_user_vaddr_bitmap( PTASK_STRUCT user_prog ) {
    user_prog->userprog_vaddr.vaddr_start = USER_VADDR_START;
    uint32_t bitmap_pg_cnt = DIV_ROUND_UP( ( 0xc0000000 - USER_VADDR_START ) / PG_SIZE / 8, PG_SIZE );
    user_prog->userprog_vaddr.vaddr_bitmap.bits = get_kernel_pages( bitmap_pg_cnt );
    user_prog->userprog_vaddr.vaddr_bitmap.btmp_bytes_len = ( 0xc0000000 - USER_VADDR_START ) / PG_SIZE / 8;
    bitmap_init( &user_prog->userprog_vaddr.vaddr_bitmap );
}

// create user process
void process_execute( void* filename, char* name ) {
    // PCB allocated in kernel memory pool
    PTASK_STRUCT thread = get_kernel_pages( 1 );
    init_thread( thread, name, default_prio );
    create_user_vaddr_bitmap( thread );
    thread_create( thread, start_process, filename );
    thread->pgdir = create_page_dir();
    block_desc_init( thread->u_block_desc );

    INTR_STATUS old_status = intr_disable();
    ASSERT( !elem_find( &thread_ready_list, &thread->general_tag ) );
    list_append( &thread_ready_list, &thread->general_tag );

    ASSERT( !elem_find( &thread_all_list, &thread->all_list_tag ) );
    list_append( &thread_all_list, &thread->all_list_tag );
    intr_set_status( old_status );
}
