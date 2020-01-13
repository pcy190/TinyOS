#include "fork.h"
#include "debug.h"
#include "file.h"
#include "interrupt.h"
#include "memory.h"
#include "process.h"
#include "string.h"
#include "thread.h"

// TODO : DEBUG FILE HEADER
#include "stdio-kernel.h"

extern void intr_exit( void );

// copy PCB , va bitmap from parent process to child process
static int32_t copy_pcb_vaddrbitmap_stack0( PTASK_STRUCT child_thread, PTASK_STRUCT parent_thread ) {
    /* a copy whole PCB page, modify it */
    memcpy( child_thread, parent_thread, PG_SIZE );
    child_thread->pid = fork_pid();
    child_thread->elapsed_ticks = 0;
    child_thread->status = TASK_READY;
    child_thread->ticks = child_thread->priority;  // full ticks
    child_thread->parent_pid = parent_thread->pid;
    child_thread->general_tag.prev = child_thread->general_tag.next = NULL;
    child_thread->all_list_tag.prev = child_thread->all_list_tag.next = NULL;
    block_desc_init( child_thread->u_block_desc );
    /* b copy va bitmap of parent process */
    uint32_t bitmap_pg_cnt = DIV_ROUND_UP( ( 0xc0000000 - USER_VADDR_START ) / PG_SIZE / 8, PG_SIZE );
    void* vaddr_btmp = get_kernel_pages( bitmap_pg_cnt );
    if ( vaddr_btmp == NULL )
        return -1;
    /* set child_thread->userprog_vaddr.vaddr_bitmap.bits  points to its thread's vaddr_btmp */
    memcpy( vaddr_btmp, child_thread->userprog_vaddr.vaddr_bitmap.bits, bitmap_pg_cnt * PG_SIZE );
    child_thread->userprog_vaddr.vaddr_bitmap.bits = vaddr_btmp;

    /* DEBUG */
    // TODO : name length check. DELETE HERE
    ASSERT( strlen( child_thread->name ) < 11 );  // pcb.name max length 16
    // strcat( child_thread->name, "_fork" );

    return 0;
}

// copy code. data and stack
static void copy_body_stack3( PTASK_STRUCT child_thread, PTASK_STRUCT parent_thread, void* buf_page ) {
    uint8_t* vaddr_btmp = parent_thread->userprog_vaddr.vaddr_bitmap.bits;
    uint32_t btmp_bytes_len = parent_thread->userprog_vaddr.vaddr_bitmap.btmp_bytes_len;
    uint32_t vaddr_start = parent_thread->userprog_vaddr.vaddr_start;
    uint32_t idx_byte = 0;
    uint32_t idx_bit = 0;
    uint32_t prog_vaddr = 0;

    // find pages in parent process memory
    while ( idx_byte < btmp_bytes_len ) {
        if ( vaddr_btmp[ idx_byte ] ) {
            idx_bit = 0;
            while ( idx_bit < 8 ) {
                if ( ( BITMAP_MASK << idx_bit ) & vaddr_btmp[ idx_byte ] ) {
                    prog_vaddr = ( idx_byte * 8 + idx_bit ) * PG_SIZE + vaddr_start;
                    /* copy parent process user data to child process  */

                    /* 1. copy parent process user data to buf_page,
                    ensure access to parent process data then switch to child process page dir*/
                    memcpy( buf_page, ( void* )prog_vaddr, PG_SIZE );

                    /* 2. switch page dir table to child process */
                    page_dir_activate( child_thread );
                    /* 3. get prog_vaddr */
                    get_a_page_without_opvaddrbitmap( PF_USER, prog_vaddr );

                    /* 4. copy from buf_page to child process memory */
                    memcpy( ( void* )prog_vaddr, buf_page, PG_SIZE );

                    /* 5. recover parent process page dir */
                    page_dir_activate( parent_thread );
                }
                idx_bit++;
            }
        }
        idx_byte++;
    }
}

// build thread stack for child process and return 0
static int32_t build_child_stack( PTASK_STRUCT child_thread ) {
    /* 1. set child process pid as 0 */
    // get child process esp0
    PINTR_STACK intr_0_stack = ( PINTR_STACK )( ( uint32_t )child_thread + PG_SIZE - sizeof( INTR_STACK ) );
    // set func return value as 0
    intr_0_stack->eax = 0;

    // 2. build thread_stack for switch_to
    uint32_t* ret_addr_in_thread_stack = ( uint32_t* )intr_0_stack - 1;

    /***   thread_stack map ***/
    // these code is no need
    uint32_t* esi_ptr_in_thread_stack = ( uint32_t* )intr_0_stack - 2;
    uint32_t* edi_ptr_in_thread_stack = ( uint32_t* )intr_0_stack - 3;
    uint32_t* ebx_ptr_in_thread_stack = ( uint32_t* )intr_0_stack - 4;
    /**********************************************************/

    uint32_t* ebp_ptr_in_thread_stack = ( uint32_t* )intr_0_stack - 5;

    /* switch_to return addr as intr_exit, i.e. return from intr */
    // printk( "intr_exit: %x\n", intr_exit );
    *ret_addr_in_thread_stack = ( uint32_t )intr_exit;

    // placeholder, no need because after intr_exit, the data will be replaced
    *ebp_ptr_in_thread_stack = *ebx_ptr_in_thread_stack = *edi_ptr_in_thread_stack = *esi_ptr_in_thread_stack = 0;
    /*********************************************************/

    // set build thread_stack stack top as stack which used when switch_to recover
    child_thread->self_kstack = ebp_ptr_in_thread_stack;
    return 0;
}

// update inode open cnt
static void update_inode_open_cnts( PTASK_STRUCT thread ) {
    int32_t local_fd = 3, global_fd = 0;
    while ( local_fd < MAX_FILES_OPEN_PER_PROC ) {
        global_fd = thread->fd_table[ local_fd ];
        ASSERT( global_fd < MAX_FILE_OPEN );
        if ( global_fd != -1 ) {
            file_table[ global_fd ].fd_inode->i_open_cnts++;
        }
        local_fd++;
    }
}

// copy parent process source to child process  ( wrapper )
static int32_t copy_process( PTASK_STRUCT child_thread, PTASK_STRUCT parent_thread ) {

    // kernel buffer
    void* buf_page = get_kernel_pages( 1 );
    if ( buf_page == NULL ) {
        return -1;
    }

    // 1. copt PCB, va bitmap, kernel stack
    if ( copy_pcb_vaddrbitmap_stack0( child_thread, parent_thread ) == -1 ) {
        return -1;
    }

    // 2. create page dir for child process
    child_thread->pgdir = create_page_dir();
    if ( child_thread->pgdir == NULL ) {
        return -1;
    }

    // 3. copy thread stack
    copy_body_stack3( child_thread, parent_thread, buf_page );

    // 4. build child thread stack and modify its pid as 0
    build_child_stack( child_thread );

    // 5. update inode cnts
    update_inode_open_cnts( child_thread );

    mfree_page( PF_KERNEL, buf_page, 1 );
    return 0;
}

// fork child process
// shouldn't be called by kernel thread
pid_t sys_fork( void ) {
    PTASK_STRUCT parent_thread = running_thread();
    PTASK_STRUCT child_thread = get_kernel_pages( 1 );  // create PCB for child process
    if ( child_thread == NULL || parent_thread == NULL ) {
        return -1;
    }
    ASSERT( INTR_OFF == intr_get_status() && parent_thread->pgdir != NULL );

    if ( copy_process( child_thread, parent_thread ) == -1 ) {
        // TODO : DEBUG INFO
        printk( "Copy from parent process fail\n" );
        return -1;
    }
    // schedule child thread
    ASSERT( !elem_find( &thread_ready_list, &child_thread->general_tag ) );
    list_append( &thread_ready_list, &child_thread->general_tag );
    ASSERT( !elem_find( &thread_all_list, &child_thread->all_list_tag ) );
    list_append( &thread_all_list, &child_thread->all_list_tag );
    // TODO : DEBUG INFO
    printk( "Finish copy from parent process with %d\n", child_thread->pid );

    return child_thread->pid;  // return child pid
}
