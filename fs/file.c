#include "file.h"
#include "inode.h"
#include "debug.h"
#include "fs.h"
#include "global.h"
#include "interrupt.h"
#include "memory.h"
#include "stdio-kernel.h"
#include "string.h"
#include "super_block.h"
#include "thread.h"

#define DEFAULT_SECS 1

// file_table
FILE file_table[ MAX_FILE_OPEN ];

// get free slot index in global file_table
// return -1 if fail
int32_t get_free_slot_in_global( void ) {
    uint32_t fd_idx = 3;
    while ( fd_idx < MAX_FILE_OPEN ) {
        if ( file_table[ fd_idx ].fd_inode == NULL ) {
            break;
        }
        fd_idx++;
    }
    if ( fd_idx == MAX_FILE_OPEN ) {
        printk( "exceed max open files\n" );
        return -1;
    }
    return fd_idx;
}

// install globa_fd_idx to process/thread's fd_table
// return index if success
// return -1 if fail
int32_t pcb_fd_install( int32_t global_fd_idx ) {
    PTASK_STRUCT cur = running_thread();
    uint8_t local_fd_idx = 3;  // skip stdin,stdout,stderr
    while ( local_fd_idx < MAX_FILES_OPEN_PER_PROC ) {
        if ( cur->fd_table[ local_fd_idx ] == -1 ) {  // free_slot
            cur->fd_table[ local_fd_idx ] = global_fd_idx;
            break;
        }
        local_fd_idx++;
    }
    if ( local_fd_idx == MAX_FILES_OPEN_PER_PROC ) {
        printk( "exceed max open files_per_proc\n" );
        return -1;
    }
    return local_fd_idx;
}

// alloc an inode and return inode index
int32_t inode_bitmap_alloc( PPARTITION part ) {
    int32_t bit_idx = bitmap_scan( &part->inode_bitmap, 1 );
    if ( bit_idx == -1 ) {
        return -1;
    }
    bitmap_set( &part->inode_bitmap, bit_idx, 1 );
    return bit_idx;
}

// alloc a block and return sector(block) addr
int32_t block_bitmap_alloc( PPARTITION part ) {
    int32_t bit_idx = bitmap_scan( &part->block_bitmap, 1 );
    if ( bit_idx == -1 ) {
        return -1;
    }
    bitmap_set( &part->block_bitmap, bit_idx, 1 );
    return ( part->sb->data_start_lba + bit_idx );  // address
}

// sync bitmap (contains bit_idx) from memory to disk
void bitmap_sync( PPARTITION part, uint32_t bit_idx, uint8_t btmp_type ) {
    uint32_t offset_sector = bit_idx / 4096;
    uint32_t offset_size = offset_sector * BLOCK_SIZE;
    uint32_t sec_lba;
    uint8_t* bitmap_off;

    // cope with INODE_BITMAP or BLOCK_BITMAP
    switch ( btmp_type ) {
    case INODE_BITMAP:
        sec_lba = part->sb->inode_bitmap_lba + offset_sector;
        bitmap_off = part->inode_bitmap.bits + offset_size;
        break;

    case BLOCK_BITMAP:
        sec_lba = part->sb->block_bitmap_lba + offset_sector;
        bitmap_off = part->block_bitmap.bits + offset_size;
        break;
    }
    // sync a sector
    ide_write( part->my_disk, sec_lba, bitmap_off, 1 );
}

// create file and return its fd
// return -1 if fail
int32_t file_create( PDIR parent_dir, char* filename, uint8_t flag ) {
    void* io_buf = sys_malloc( 1024 );  // buffer
    if ( io_buf == NULL ) {
        printk( "in file_creat: sys_malloc for io_buf failed\n" );
        return -1;
    }

    uint8_t rollback_step = 0;  // rollback step

    /* alloc inode */
    int32_t inode_no = inode_bitmap_alloc( cur_part );
    if ( inode_no == -1 ) {
        printk( "in file_creat: allocate inode failed\n" );
        return -1;
    }

    PINODE new_file_inode = ( PINODE )sys_malloc( sizeof( INODE ) );
    if ( new_file_inode == NULL ) {
        printk( "file_create: sys_malloc for inode failded\n" );
        rollback_step = 1;
        goto rollback;
    }
    inode_init( inode_no, new_file_inode );  // init inode

    /* return file_table index */
    int fd_idx = get_free_slot_in_global();
    if ( fd_idx == -1 ) {
        printk( "exceed max open files\n" );
        rollback_step = 2;
        goto rollback;
    }

    file_table[ fd_idx ].fd_inode = new_file_inode;
    file_table[ fd_idx ].fd_pos = 0;
    file_table[ fd_idx ].fd_flag = flag;
    file_table[ fd_idx ].fd_inode->write_deny = false;

    DIR_ENTRY new_dir_entry;
    memset( &new_dir_entry, 0, sizeof( DIR_ENTRY ) );

    create_dir_entry( filename, inode_no, FT_REGULAR, &new_dir_entry );  // create_dir_entry can't cause error

    /* sync data to disk */
    /* 1. new_dir_entry is installed in parent_dir, sync parent_dir to disk*/
    if ( !sync_dir_entry( parent_dir, &new_dir_entry, io_buf ) ) {
        printk( "sync dir_entry to disk failed\n" );
        rollback_step = 3;
        goto rollback;
    }

    memset( io_buf, 0, 1024 );
    /* 2. sync parent dir inode to disk */
    inode_sync( cur_part, parent_dir->inode, io_buf );

    memset( io_buf, 0, 1024 );
    /* 3. sync file inode to disk */
    inode_sync( cur_part, new_file_inode, io_buf );

    /* 4. sync inode_bitmap */
    bitmap_sync( cur_part, inode_no, INODE_BITMAP );

    /* 5. append new file inode to open_inodes list */
    list_push( &cur_part->open_inodes, &new_file_inode->inode_tag );
    new_file_inode->i_open_cnts = 1;

    sys_free( io_buf );
    return pcb_fd_install( fd_idx );

/* error handler: rollback to recover resources */
rollback:
    switch ( rollback_step ) {
    case 3:
        /* reset file_table */
        memset( &file_table[ fd_idx ], 0, sizeof( FILE ) );
    case 2:
        sys_free( new_file_inode );
    case 1:
        /* reset inode_no in inode bitmap */
        bitmap_set( &cur_part->inode_bitmap, inode_no, 0 );
        break;
    }
    sys_free( io_buf );
    return -1;
}
