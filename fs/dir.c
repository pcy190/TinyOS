#include "dir.h"
#include "debug.h"
#include "file.h"
#include "fs.h"
#include "global.h"
#include "inode.h"
#include "interrupt.h"
#include "memory.h"
#include "stdint.h"
#include "stdio-kernel.h"
#include "string.h"
#include "super_block.h"

DIR root_dir;  // root directory, static

// open_root_dir
void open_root_dir( PPARTITION part ) {
    root_dir.inode = inode_open( part, part->sb->root_inode_no );
    root_dir.dir_pos = 0;
}

// open inode of part , return dir pointer
PDIR dir_open( PPARTITION part, uint32_t inode_no ) {
    PDIR pdir = ( PDIR )sys_malloc( sizeof( DIR ) );
    pdir->inode = inode_open( part, inode_no );
    pdir->dir_pos = 0;
    return pdir;
}

// find name file in pdir of part(partition)
// if find, store its directory table to dir_e and return true
// if fail, return false
bool search_dir_entry( PPARTITION part, PDIR pdir, const char* name, PDIR_ENTRY dir_e ) {
    uint32_t block_cnt = 140;  // 12 driect blocks +128 primary indirect blocks ===> 140 blocks

    // TODO : valid name check
    ASSERT( strlen( name ) <= MAX_FILE_NAME_LEN );

    // blocks: 12*4 + 128*4 bytes
    uint32_t* all_blocks = ( uint32_t* )sys_malloc( 48 + 512 );
    if ( all_blocks == NULL ) {
        printk( "Search_dir_entry: sys_malloc for all_blocks failed!\n" );
        return false;
    }

    uint32_t block_idx = 0;
    while ( block_idx < 12 ) {
        all_blocks[ block_idx ] = pdir->inode->i_sectors[ block_idx ];
        block_idx++;
    }
    block_idx = 0;

    if ( pdir->inode->i_sectors[ 12 ] != 0 ) {
        // cope with primary indirect blocks
        printk( "Print pdir12th:  %d", pdir->inode->i_sectors[ 12 ] );
        ide_read( part->my_disk, pdir->inode->i_sectors[ 12 ], all_blocks + 12, 1 );
    }
    // all_blocks stores all entry tables

    uint8_t* buf = ( uint8_t* )sys_malloc( SECTOR_SIZE );
    PDIR_ENTRY p_de = ( PDIR_ENTRY )buf;  // p_de points to dir table
    uint32_t dir_entry_size = part->sb->dir_entry_size;
    uint32_t dir_entry_cnt = SECTOR_SIZE / dir_entry_size;  // entry count in a sector

    // find in all blocks
    while ( block_idx < block_cnt ) {
        if ( all_blocks[ block_idx ] == 0 ) {
            block_idx++;
            continue;
        }
        ide_read( part->my_disk, all_blocks[ block_idx ], buf, 1 );

        uint32_t dir_entry_idx = 0;
        // iterate all enry
        while ( dir_entry_idx < dir_entry_cnt ) {
            // find it and copy to dir_e
            if ( !strcmp( p_de->filename, name ) ) {
                memcpy( dir_e, p_de, dir_entry_size );
                sys_free( buf );
                sys_free( all_blocks );
                return true;
            }
            dir_entry_idx++;
            p_de++;
        }
        block_idx++;
        p_de = ( PDIR_ENTRY )buf;       // p_de points to the last entry of the sector, reset it
        memset( buf, 0, SECTOR_SIZE );  // reset to iterate next block
    }
    sys_free( buf );
    sys_free( all_blocks );
    return false;
}

// close the directory
void dir_close( PDIR dir ) {

    // SHOULDN'T close root dir
    // root_dir in least 1M memory, we cann't free it.
    if ( dir == &root_dir ) {
        // skip root dir
        return;
    }
    inode_close( dir->inode );
    sys_free( dir );
}

// init dir entry (p_de)
void create_dir_entry( char* filename, uint32_t inode_no, uint8_t file_type, PDIR_ENTRY p_de ) {
    ASSERT( strlen( filename ) <= MAX_FILE_NAME_LEN );

    memcpy( p_de->filename, filename, strlen( filename ) );
    p_de->i_no = inode_no;
    p_de->f_type = file_type;
}

// write dir entry to parent dir.
// caller should provide io_buf
bool sync_dir_entry( PDIR parent_dir, PDIR_ENTRY p_de, void* io_buf ) {
    PINODE dir_inode = parent_dir->inode;
    uint32_t dir_size = dir_inode->i_size;
    uint32_t dir_entry_size = cur_part->sb->dir_entry_size;

    ASSERT( dir_size % dir_entry_size == 0 );

    uint32_t dir_entrys_per_sec = ( 512 / dir_entry_size );  // max entries of each directory
    int32_t block_lba = -1;

    // store 140 blocks(12 driect blocks +128 primary indirect blocks) to all_blocks
    uint8_t block_idx = 0;
    uint32_t all_blocks[ 140 ] = {0};  // all blocks
    while ( block_idx < 12 ) {
        all_blocks[ block_idx ] = dir_inode->i_sectors[ block_idx ];
        block_idx++;
    }

    PDIR_ENTRY dir_e = ( PDIR_ENTRY )io_buf;
    int32_t block_bitmap_idx = -1;

    // find the slot of the directory table
    // alloc new sector if current sector is full

    block_idx = 0;
    while ( block_idx < 140 ) {  // support up to 140 blocks
        block_bitmap_idx = -1;
        if ( all_blocks[ block_idx ] == 0 ) {
            block_lba = block_bitmap_alloc( cur_part );
            if ( block_lba == -1 ) {
                printk( "alloc block bitmap for sync_dir_entry failed\n" );
                return false;
            }

            // sync block_bitmap after each bitmap alloc
            block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
            ASSERT( block_bitmap_idx != -1 );
            bitmap_sync( cur_part, block_bitmap_idx, BLOCK_BITMAP );

            block_bitmap_idx = -1;
            if ( block_idx < 12 ) {  // direct block
                dir_inode->i_sectors[ block_idx ] = all_blocks[ block_idx ] = block_lba;
            } else if ( block_idx == 12 ) {              // primary indirect block
                dir_inode->i_sectors[ 12 ] = block_lba;  // use block_lba as primary indirect block
                block_lba = -1;
                block_lba = block_bitmap_alloc( cur_part );  // alloc 0th indirect block
                if ( block_lba == -1 ) {
                    block_bitmap_idx = dir_inode->i_sectors[ 12 ] - cur_part->sb->data_start_lba;
                    bitmap_set( &cur_part->block_bitmap, block_bitmap_idx, 0 );
                    dir_inode->i_sectors[ 12 ] = 0;
                    printk( "alloc block bitmap for sync_dir_entry failed\n" );
                    return false;
                }

                // sync block_bitmap
                block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
                ASSERT( block_bitmap_idx != -1 );
                bitmap_sync( cur_part, block_bitmap_idx, BLOCK_BITMAP );

                all_blocks[ 12 ] = block_lba;
                // write 0th indirect block addr to primary indirect block entry table
                // TODO : addr test
                ide_write( cur_part->my_disk, dir_inode->i_sectors[ 12 ], all_blocks + 12, 1 );
            } else {  // indirect block
                all_blocks[ block_idx ] = block_lba;
                // write (block_idx-12)th indirect block address to primary block table
                ide_write( cur_part->my_disk, dir_inode->i_sectors[ 12 ], all_blocks + 12, 1 );
            }

            // write p_de to new indirect block
            memset( io_buf, 0, 512 );
            memcpy( io_buf, p_de, dir_entry_size );
            ide_write( cur_part->my_disk, all_blocks[ block_idx ], io_buf, 1 );
            dir_inode->i_size += dir_entry_size;
            return true;
        }

        // if block_idx block exists, find slot dir entry in this block
        ide_read( cur_part->my_disk, all_blocks[ block_idx ], io_buf, 1 );
        // find slot dir entry
        uint8_t dir_entry_idx = 0;
        while ( dir_entry_idx < dir_entrys_per_sec ) {
            // judge the empty dir entry from the f_type
            // FT_UNKNOWN==0, i.e. after init or delete, f_type is FT_UNKNOWN.
            if ( ( dir_e + dir_entry_idx )->f_type == FT_UNKNOWN ) {
                memcpy( dir_e + dir_entry_idx, p_de, dir_entry_size );
                ide_write( cur_part->my_disk, all_blocks[ block_idx ], io_buf, 1 );

                dir_inode->i_size += dir_entry_size;
                return true;
            }
            dir_entry_idx++;
        }
        block_idx++;
    }
    printk( "Directory is full!\n" );
    return false;
}
