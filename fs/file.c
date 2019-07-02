#include "file.h"
#include "debug.h"
#include "fs.h"
#include "global.h"
#include "inode.h"
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

// open inode file and return fd in pcb
int32_t file_open( uint32_t inode_no, uint8_t flag ) {
    int fd_idx = get_free_slot_in_global();
    if ( fd_idx == -1 ) {
        printk( "Exceed max open files\n" );
        return -1;
    }
    file_table[ fd_idx ].fd_inode = inode_open( cur_part, inode_no );
    file_table[ fd_idx ].fd_pos = 0;  // reset fd_pos to 0, i.e. point to file head when initially open file
    file_table[ fd_idx ].fd_flag = flag;
    bool* write_deny = &file_table[ fd_idx ].fd_inode->write_deny;

    if ( flag & O_WRONLY || flag & O_RDWR ) {  // judge write
        INTR_STATUS old_status = intr_disable();
        if ( !( *write_deny ) ) {
            *write_deny = true;  // set write deny
            intr_set_status( old_status );
        } else {  // since occupied, write deny fail
            intr_set_status( old_status );
            printk( "File cannot be write now, try again later\n" );
            return -1;
        }
    }
    return pcb_fd_install( fd_idx );
}

// close file
int32_t file_close( PFILE file ) {
    if ( file == NULL ) {
        return -1;
    }
    file->fd_inode->write_deny = false;
    inode_close( file->fd_inode );
    file->fd_inode = NULL;
    return 0;
}

/*  write count bytes from buf to file
    return written bytes number if success
    return -1 if fail
 */
int32_t file_write( PFILE file, const void* buf, uint32_t count ) {
    if ( ( file->fd_inode->i_size + count ) > ( BLOCK_SIZE * 140 ) ) {  // file maximum support 512*140=71680 bytes
        printk( "Exceed max file_size 71680 bytes, write file failed\n" );
        return -1;
    }
    uint8_t* io_buf = sys_malloc( BLOCK_SIZE );
    if ( io_buf == NULL ) {
        printk( "file_write: sys_malloc for io_buf failed\n" );
        return -1;
    }
    uint32_t* all_blocks = ( uint32_t* )sys_malloc( BLOCK_SIZE + 48 );  // record all blocks addr
    if ( all_blocks == NULL ) {
        printk( "file_write: sys_malloc for all_blocks failed\n" );
        return -1;
    }

    const uint8_t* src = buf;       // src points to original data buffer
    uint32_t bytes_written = 0;     // written bytes size
    uint32_t size_left = count;     // left bytes size
    int32_t block_lba = -1;         // block lba address
    uint32_t block_bitmap_idx = 0;  // block index in block_bitmap, used when bitmap_sync
    uint32_t sec_idx;               // sector index
    uint32_t sec_lba;               // sector lba address
    uint32_t sec_off_bytes;         // bytes offset in sector
    uint32_t sec_left_bytes;        // left bytes in sector
    uint32_t chunk_size;            // data size each time write to disk
    int32_t indirect_block_table;   // primary indirect block table
    uint32_t block_idx;             // block index

    // judge file first write. alloc a block if first write
    if ( file->fd_inode->i_sectors[ 0 ] == 0 ) {
        block_lba = block_bitmap_alloc( cur_part );
        if ( block_lba == -1 ) {
            printk( "file_write: block_bitmap_alloc failed\n" );
            return -1;
        }
        file->fd_inode->i_sectors[ 0 ] = block_lba;

        // sync the alloced block to disk
        block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
        ASSERT( block_bitmap_idx != 0 );
        bitmap_sync( cur_part, block_bitmap_idx, BLOCK_BITMAP );
    }

    // occupied blocks number before write
    uint32_t file_has_used_blocks = file->fd_inode->i_size / BLOCK_SIZE + 1;

    // occupied blocks number after write
    uint32_t file_will_use_blocks = ( file->fd_inode->i_size + count ) / BLOCK_SIZE + 1;
    ASSERT( file_will_use_blocks <= 140 );

    // whether need new blocks to store
    uint32_t add_blocks = file_will_use_blocks - file_has_used_blocks;

    // store all blocks address to all_blocks. i.e. sectors address
    if ( add_blocks == 0 ) {
        // write data in the same sector(block)
        if ( file_has_used_blocks <= 12 ) {
            block_idx = file_has_used_blocks - 1;  // point to the last used block
            all_blocks[ block_idx ] = file->fd_inode->i_sectors[ block_idx ];
        } else {
            // use indirect block
            ASSERT( file->fd_inode->i_sectors[ 12 ] != 0 );
            indirect_block_table = file->fd_inode->i_sectors[ 12 ];
            ide_read( cur_part->my_disk, indirect_block_table, all_blocks + 12, 1 );
        }
    } else {
        // Three cases:
        /* 1st case: 12 direct block is enough */
        if ( file_will_use_blocks <= 12 ) {
            /* write spare sector address to all_blocks */
            block_idx = file_has_used_blocks - 1;
            ASSERT( file->fd_inode->i_sectors[ block_idx ] != 0 );
            all_blocks[ block_idx ] = file->fd_inode->i_sectors[ block_idx ];

            /* write needed sectors address all_blocks */
            block_idx = file_has_used_blocks;  // new blocks index to alloc
            while ( block_idx < file_will_use_blocks ) {
                block_lba = block_bitmap_alloc( cur_part );
                if ( block_lba == -1 ) {
                    printk( "file_write: block_bitmap_alloc for case 1 failed\n" );
                    return -1;
                }

                // ensure init state in unused sector
                ASSERT( file->fd_inode->i_sectors[ block_idx ] == 0 );
                file->fd_inode->i_sectors[ block_idx ] = all_blocks[ block_idx ] = block_lba;

                // sync the alloced block to disk
                block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
                bitmap_sync( cur_part, block_bitmap_idx, BLOCK_BITMAP );

                block_idx++;  // next block index to alloc
            }
        } else if ( file_has_used_blocks <= 12 && file_will_use_blocks > 12 ) {
            // 2nd case : old data in direct block, new data will use indirect block
            block_idx = file_has_used_blocks - 1;
            all_blocks[ block_idx ] = file->fd_inode->i_sectors[ block_idx ];

            // create indirect block table
            block_lba = block_bitmap_alloc( cur_part );
            if ( block_lba == -1 ) {
                printk( "file_write: block_bitmap_alloc for case 2 failed\n" );
                return -1;
            }

            ASSERT( file->fd_inode->i_sectors[ 12 ] == 0 );  // ensure indirect block not alloced
            // alloc indirect block table
            indirect_block_table = file->fd_inode->i_sectors[ 12 ] = block_lba;

            block_idx = file_has_used_blocks;
            while ( block_idx < file_will_use_blocks ) {
                block_lba = block_bitmap_alloc( cur_part );
                if ( block_lba == -1 ) {
                    printk( "file_write: block_bitmap_alloc for case 2 failed\n" );
                    return -1;
                }

                if ( block_idx < 12 ) {  // new 0~11th block store to all_blocks
                    ASSERT( file->fd_inode->i_sectors[ block_idx ] == 0 );
                    file->fd_inode->i_sectors[ block_idx ] = all_blocks[ block_idx ] = block_lba;
                } else {
                    all_blocks[ block_idx ] = block_lba;
                }

                block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
                bitmap_sync( cur_part, block_bitmap_idx, BLOCK_BITMAP );

                block_idx++;
            }
            ide_write( cur_part->my_disk, indirect_block_table, all_blocks + 12, 1 );  // sync indirect block table to disk
        } else if ( file_has_used_blocks > 12 ) {
            // 3rd case: new data use indirect block
            ASSERT( file->fd_inode->i_sectors[ 12 ] != 0 );          
            indirect_block_table = file->fd_inode->i_sectors[ 12 ];  // get indirect block table address

            // get all indirect blocks addr
            ide_read( cur_part->my_disk, indirect_block_table, all_blocks + 12, 1 );  

            block_idx = file_has_used_blocks;
            while ( block_idx < file_will_use_blocks ) {
                block_lba = block_bitmap_alloc( cur_part );
                if ( block_lba == -1 ) {
                    printk( "file_write: block_bitmap_alloc for case 3 failed\n" );
                    return -1;
                }
                all_blocks[ block_idx++ ] = block_lba;

                block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
                bitmap_sync( cur_part, block_bitmap_idx, BLOCK_BITMAP );
            }
            ide_write( cur_part->my_disk, indirect_block_table, all_blocks + 12, 1 );  // sync indirect block table
        }
    }

    bool first_write_block = true;
    /* block address record to all_blocks */
    file->fd_pos = file->fd_inode->i_size - 1;  
    while ( bytes_written < count ) {           // write all data
        memset( io_buf, 0, BLOCK_SIZE );
        sec_idx = file->fd_inode->i_size / BLOCK_SIZE;
        sec_lba = all_blocks[ sec_idx ];
        sec_off_bytes = file->fd_inode->i_size % BLOCK_SIZE;
        sec_left_bytes = BLOCK_SIZE - sec_off_bytes;

        /* write chunk size */
        chunk_size = size_left < sec_left_bytes ? size_left : sec_left_bytes;
        if ( first_write_block ) {
            ide_read( cur_part->my_disk, sec_lba, io_buf, 1 );
            first_write_block = false;
        }
        memcpy( io_buf + sec_off_bytes, src, chunk_size );
        ide_write( cur_part->my_disk, sec_lba, io_buf, 1 );
        // TODO : debug test 
        printk( "file write at lba 0x%x\n", sec_lba );

        src += chunk_size;                     // update src point
        file->fd_inode->i_size += chunk_size;  // update size
        file->fd_pos += chunk_size;
        bytes_written += chunk_size;
        size_left -= chunk_size;
    }
    inode_sync( cur_part, file->fd_inode, io_buf );
    sys_free( all_blocks );
    sys_free( io_buf );
    return bytes_written;
}
