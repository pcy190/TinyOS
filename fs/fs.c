#include "fs.h"
#include "debug.h"
#include "dir.h"
#include "file.h"
#include "global.h"
#include "ide.h"
#include "inode.h"
#include "list.h"
#include "memory.h"
#include "stdint.h"
#include "stdio-kernel.h"
#include "string.h"
#include "super_block.h"

PPARTITION cur_part;  // current partition cursor

// find the part_name partition and assign the cur to cur_part
static bool mount_partition( PLIST_NODE pelem, void* arg ) {
    char* part_name = ( char* )arg;
    PPARTITION part = elem2entry( PARTITION, part_tag, pelem );
    if ( !strcmp( part->name, part_name ) ) {
        cur_part = part;
        struct disk* hd = cur_part->my_disk;

        // malloc sb_buf to store super block from disk
        PSUPER_BLOCK sb_buf = ( PSUPER_BLOCK )sys_malloc( SECTOR_SIZE );

        // create super block of cur_part in memory
        cur_part->sb = ( PSUPER_BLOCK )sys_malloc( sizeof( SUPER_BLOCK ) );
        if ( cur_part->sb == NULL ) {
            PANIC( "alloc memory failed!" );
        }

        // read super block
        memset( sb_buf, 0, SECTOR_SIZE );
        ide_read( hd, cur_part->start_lba + 1, sb_buf, 1 );

        // copy sb_buf super block to partition's super block
        memcpy( cur_part->sb, sb_buf, sizeof( SUPER_BLOCK ) );

        // read disk's bitmap to memory
        cur_part->block_bitmap.bits = ( uint8_t* )sys_malloc( sb_buf->block_bitmap_sectors * SECTOR_SIZE );
        if ( cur_part->block_bitmap.bits == NULL ) {
            PANIC( "alloc memory failed!" );
        }
        cur_part->block_bitmap.btmp_bytes_len = sb_buf->block_bitmap_sectors * SECTOR_SIZE;
        // read bitmap from disk to block_bitmap.bits
        ide_read( hd, sb_buf->block_bitmap_lba, cur_part->block_bitmap.bits, sb_buf->block_bitmap_sectors );
        /********************************************************************/

        /**********     read inode bitmap from disk to memory    ************/
        printk( "Going to malloc %d  %d\n", sb_buf->inode_bitmap_sectors, sb_buf->inode_bitmap_sectors * SECTOR_SIZE );
        cur_part->inode_bitmap.bits = ( uint8_t* )sys_malloc( sb_buf->inode_bitmap_sectors * SECTOR_SIZE );
        if ( cur_part->inode_bitmap.bits == NULL ) {
            PANIC( "alloc memory failed!" );
        }
        cur_part->inode_bitmap.btmp_bytes_len = sb_buf->inode_bitmap_sectors * SECTOR_SIZE;
        // read inode bitmap from disk to inode_bitmap.bits
        ide_read( hd, sb_buf->inode_bitmap_lba, cur_part->inode_bitmap.bits, sb_buf->inode_bitmap_sectors );
        /********************************************************************/

        list_init( &cur_part->open_inodes );
        printk( "mount %s done!\n", part->name );
        // return true to stop list_traversal because we have coped with the
        // cur_part
        return true;
    }
    return false;  // return false to continue list_traversal
}

// format partition, create file system
static void partition_format( PPARTITION part ) {
    uint32_t boot_sector_sects = 1;
    uint32_t super_block_sects = 1;
    // bitmap support maximum 4096 files
    uint32_t inode_bitmap_sects = DIV_ROUND_UP( MAX_FILES_PER_PART, BITS_PER_SECTOR );
    uint32_t inode_table_sects = DIV_ROUND_UP( ( ( sizeof( INODE ) * MAX_FILES_PER_PART ) ), SECTOR_SIZE );
    uint32_t used_sects = boot_sector_sects + super_block_sects + inode_bitmap_sects + inode_table_sects;
    uint32_t free_sects = part->sec_cnt - used_sects;

    /************** calc block bitmap's sectors ***************/
    uint32_t block_bitmap_sectors;
    block_bitmap_sectors = DIV_ROUND_UP( free_sects, BITS_PER_SECTOR );
    // block_bitmap_bit_len: availble blocks number
    uint32_t block_bitmap_bit_len = free_sects - block_bitmap_sectors;
    block_bitmap_sectors = DIV_ROUND_UP( block_bitmap_bit_len, BITS_PER_SECTOR );
    /*********************************************************/

    // init super block
    SUPER_BLOCK sb;
    sb.magic = FS_MAGIC;
    sb.sec_cnt = part->sec_cnt;
    sb.inode_cnt = MAX_FILES_PER_PART;
    sb.part_lba_base = part->start_lba;
    // TODO : lba base offset
    sb.block_bitmap_lba = sb.part_lba_base + 2;  // 0th block: boot block, 1st block: super block
    sb.block_bitmap_sectors = block_bitmap_sectors;

    sb.inode_bitmap_lba = sb.block_bitmap_lba + sb.block_bitmap_sectors;
    sb.inode_bitmap_sectors = inode_bitmap_sects;

    sb.inode_table_lba = sb.inode_bitmap_lba + sb.inode_bitmap_sectors;
    sb.inode_table_sectors = inode_table_sects;

    sb.data_start_lba = sb.inode_table_lba + sb.inode_table_sectors;
    sb.root_inode_no = 0;
    sb.dir_entry_size = sizeof( DIR_ENTRY );

    printk( "%s info:\n", part->name );
    printk( "   magic:0x%x\n   part_lba_base:0x%x\n   all_sectors:0x%x\n   "
            "inode_cnt:0x%x\n   block_bitmap_lba:0x%x\n   "
            "block_bitmap_sectors:0x%x\n   inode_bitmap_lba:0x%x\n   "
            "inode_bitmap_sectors:0x%x\n   inode_table_lba:0x%x\n   "
            "inode_table_sectors:0x%x\n   data_start_lba:0x%x\n",
            sb.magic, sb.part_lba_base, sb.sec_cnt, sb.inode_cnt, sb.block_bitmap_lba, sb.block_bitmap_sectors, sb.inode_bitmap_lba, sb.inode_bitmap_sectors, sb.inode_table_lba,
            sb.inode_table_sectors, sb.data_start_lba );

    PDISK hd = part->my_disk;
    /*******************************
     * 1 write super block to 1th sectors *
     ******************************/
    ide_write( hd, part->start_lba + 1, &sb, 1 );
    printk( "   super_block_lba:0x%x\n", part->start_lba + 1 );

    // get buf size
    uint32_t buf_size = ( sb.block_bitmap_sectors >= sb.inode_bitmap_sectors ? sb.block_bitmap_sectors : sb.inode_bitmap_sectors );
    buf_size = ( buf_size >= sb.inode_table_sectors ? buf_size : sb.inode_table_sectors ) * SECTOR_SIZE;
    uint8_t* buf = ( uint8_t* )sys_malloc( buf_size );  // zero set memory

    /**************************************
     * 2 init block bitmap and write to sb.block_bitmap_lba *
     *************************************/
    // init block_bitmap
    buf[ 0 ] |= 0x01;  // 0th block reserved to / (root dir)
    uint32_t block_bitmap_last_byte = block_bitmap_bit_len / 8;
    uint8_t block_bitmap_last_bit = block_bitmap_bit_len % 8;
    uint32_t last_size = SECTOR_SIZE - ( block_bitmap_last_byte % SECTOR_SIZE );  // last_size: the remain size of last sector

    // 1 we declare the part which out of the block bitmap range is used
    // (set bit from last bitmap to sector end as 1)
    memset( &buf[ block_bitmap_last_byte ], 0xff, last_size );

    // 2 reset bits in last byte as 0
    uint8_t bit_idx = 0;
    while ( bit_idx <= block_bitmap_last_bit ) {
        buf[ block_bitmap_last_byte ] &= ~( 1 << bit_idx++ );
    }
    ide_write( hd, sb.block_bitmap_lba, buf, sb.block_bitmap_sectors );

    /***************************************
     * 3 init inode bitmap and write to sb.inode_bitmap_lba *
     ***************************************/
    memset( buf, 0, buf_size );
    buf[ 0 ] |= 0x1;  // 0th inode: used by root dir
    /* 4096 inode in inode_table. inode_bitmap occupy 1 sector
       i.e. inode_bitmap_sectors==1
       so we don't need to deal the more used bit like block_bitmap
     */
    ide_write( hd, sb.inode_bitmap_lba, buf, sb.inode_bitmap_sectors );

    /***************************************
     * 4 init inode array and write to sb.inode_table_lba *
     ***************************************/
    /* write inode_table 0th table (root dir)*/
    // set buf as 0 and ensure inode is empty
    memset( buf, 0, buf_size );
    PINODE i = ( PINODE )buf;
    i->i_size = sb.dir_entry_size * 2;      // . and .. dir
    i->i_number = 0;                        // root dir is the 0th inode
    i->i_sectors[ 0 ] = sb.data_start_lba;  // 0th data block
    ide_write( hd, sb.inode_table_lba, buf, sb.inode_table_sectors );

    /***************************************
     * 5 init root dir and write to sb.data_start_lba
     ***************************************/
    // write . and ..
    memset( buf, 0, buf_size );
    DIR_ENTRY* p_de = ( DIR_ENTRY* )buf;

    // init . dir
    memcpy( p_de->filename, ".", 1 );
    p_de->i_no = 0;
    p_de->f_type = FT_DIRECTORY;
    p_de++;

    // init .. dir
    memcpy( p_de->filename, "..", 2 );
    p_de->i_no = 0;  // root's parent dir is root
    p_de->f_type = FT_DIRECTORY;

    // sb.data_start_lba contains root dir tables
    ide_write( hd, sb.data_start_lba, buf, 1 );

    printk( "   root_dir_lba:0x%x\n", sb.data_start_lba );
    printk( "%s format done\n", part->name );
    sys_free( buf );
}

// parse the top path
// strip repeated / char
static char* path_parse( char* pathname, char* name_res ) {
    uint32_t name_length = 0;
    if ( pathname[ 0 ] == '/' ) {  // parse and skip root directory
        while ( *( ++pathname ) == '/' )
            ;
    }

    while ( *pathname != '/' && *pathname != 0 ) {
        *name_res++ = *pathname++;
        name_length++;
    }

    // TODO : name length check
    if ( name_length > MAX_FILE_NAME_LEN ) {
        printk( "File name is too long while path parse!\n" );
    }

    if ( pathname[ 0 ] == 0 ) {
        return NULL;
    }
    return pathname;
}

// get the path depth
int32_t path_depth_cnt( char* pathname ) {
    ASSERT( pathname != NULL );
    char* p = pathname;
    char name[ MAX_FILE_NAME_LEN ];
    uint32_t depth = 0;

    // parse each depth name
    p = path_parse( p, name );
    while ( name[ 0 ] ) {
        depth++;
        memset( name, 0, MAX_FILE_NAME_LEN );
        if ( p ) {  // deal until pathname ends
            p = path_parse( p, name );
        }
    }
    return depth;
}

// search file pathname, return inode if found, otherwise return -1
static int search_file( const char* pathname, PPATH_SEARCH_RECORD searched_record ) {
    // root directory case
    char* root_dir_match_table[] = {"/", "/.", "/.."};
    int rdir_idx = 0;
    for ( rdir_idx = 0; rdir_idx < sizeof( root_dir_match_table ) / sizeof( char* ); rdir_idx++ ) {
        if ( !strcmp( pathname, root_dir_match_table[ rdir_idx ] ) ) {
            searched_record->parent_dir = &root_dir;
            searched_record->file_type = FT_DIRECTORY;
            searched_record->searched_path[ 0 ] = 0;  // empty path
            return 0;
        }
    }
    uint32_t path_len = strlen( pathname );

    // ensure absolute pathname
    ASSERT( pathname[ 0 ] == '/' && path_len > 1 && path_len < MAX_PATH_LEN );
    char* sub_path = ( char* )pathname;
    PDIR parent_dir = &root_dir;
    DIR_ENTRY dir_e;

    // record each depth's path name
    char name[ MAX_FILE_NAME_LEN ] = {0};

    searched_record->parent_dir = parent_dir;
    searched_record->file_type = FT_UNKNOWN;
    uint32_t parent_inode_no = 0;

    sub_path = path_parse( sub_path, name );
    while ( name[ 0 ] ) {
        ASSERT( strlen( searched_record->searched_path ) < 512 );

        // append to searched_path
        strcat( searched_record->searched_path, "/" );
        strcat( searched_record->searched_path, name );
        // find name file in parent_dir
        if ( search_dir_entry( cur_part, parent_dir, name, &dir_e ) ) {
            memset( name, 0, MAX_FILE_NAME_LEN );
            // deal sub_path to end
            if ( sub_path ) {
                sub_path = path_parse( sub_path, name );
            }

            if ( FT_DIRECTORY == dir_e.f_type ) {  // directory
                parent_inode_no = parent_dir->inode->i_number;
                dir_close( parent_dir );
                parent_dir = dir_open( cur_part, dir_e.i_no );  // update open parent dir
                searched_record->parent_dir = parent_dir;
                continue;
            } else if ( FT_REGULAR == dir_e.f_type ) {  // ordinary file
                searched_record->file_type = FT_REGULAR;
                return dir_e.i_no;
            }
        } else {
            // couldn't find
            // reserve parent_dir open
            return -1;
        }
    }
    // the end path is directory
    dir_close( searched_record->parent_dir );

    // store parent dir
    searched_record->parent_dir = dir_open( cur_part, parent_inode_no );
    searched_record->file_type = FT_DIRECTORY;
    return dir_e.i_no;
}

// open/create file
// return fd number (success)
// return -1 (fail)
int32_t sys_open( const char* pathname, uint8_t flags ) {
    // can't open dir
    if ( pathname[ strlen( pathname ) - 1 ] == '/' ) {
        printk( "can`t open a directory %s\n", pathname );
        return -1;
    }
    ASSERT( flags <= 7 );
    int32_t fd = -1;  // default cannot find

    PATH_SEARCH_RECORD searched_record;
    memset( &searched_record, 0, sizeof( PATH_SEARCH_RECORD ) );

    /* record dir depth */
    uint32_t pathname_depth = path_depth_cnt( ( char* )pathname );

    /* search file */
    int inode_no = search_file( pathname, &searched_record );
    bool found = inode_no != -1 ? true : false;

    if ( searched_record.file_type == FT_DIRECTORY ) {
        printk( "Can`t open a direcotry with open(), use opendir() instead\n" );
        dir_close( searched_record.parent_dir );
        return -1;
    }

    uint32_t path_searched_depth = path_depth_cnt( searched_record.searched_path );

    /* judge pathname exists */
    if ( pathname_depth != path_searched_depth ) {  // some path not exist
        printk( "cannot access %s: Not a directory, subpath %s is`t exist\n", pathname, searched_record.searched_path );
        dir_close( searched_record.parent_dir );
        return -1;
    }

    if ( !found && !( flags & O_CREAT ) ) {
        printk( "in path %s, file %s is`t exist\n", searched_record.searched_path, ( strrchr( searched_record.searched_path, '/' ) + 1 ) );
        dir_close( searched_record.parent_dir );
        return -1;
    } else if ( found && flags & O_CREAT ) {  // exist can't create
        printk( "%s has already exist!\n", pathname );
        dir_close( searched_record.parent_dir );
        return -1;
    }
    switch ( flags & O_CREAT ) {
    case O_CREAT:
        printk( "creating file\n" );
        fd = file_create( searched_record.parent_dir, ( strrchr( pathname, '/' ) + 1 ), flags );
        dir_close( searched_record.parent_dir );
        break;
    default:
        /* exist file:
         * O_RDONLY,O_WRONLY,O_RDWR */
        fd = file_open( inode_no, flags );
    }

    // here fd is pcb->fd_table index
    // not global file_table index
    return fd;
}

// convert local fd to global file table index
static uint32_t fd_local2global( uint32_t local_fd ) {
    PTASK_STRUCT cur = running_thread();
    int32_t global_fd = cur->fd_table[ local_fd ];
    ASSERT( global_fd >= 0 && global_fd < MAX_FILE_OPEN );
    return ( uint32_t )global_fd;
}

// close fd file
// return -1 if fail, 0 if success
int32_t sys_close( int32_t fd ) {
    int32_t ret = -1;  // default fail
    if ( fd > 2 ) {
        uint32_t _fd = fd_local2global( fd );
        ret = file_close( &file_table[ _fd ] );
        running_thread()->fd_table[ fd ] = -1;  // make fd slot
    }
    return ret;
}

/*  write count bytes from buf to fd
    return written bytes if success
    return -1 if fail
    */
int32_t sys_write( int32_t fd, const void* buf, uint32_t count ) {
    if ( fd < 0 ) {
        printk( "sys_write: fd error\n" );
        return -1;
    }
    if ( fd == FD_STDOUT ) {
        char tmp_buf[ 1024 ] = {0};
        memcpy( tmp_buf, buf, count );
        console_put_str( tmp_buf );
        return count;
    }
    uint32_t _fd = fd_local2global( fd );
    PFILE wr_file = &file_table[ _fd ];
    if ( wr_file->fd_flag & O_WRONLY || wr_file->fd_flag & O_RDWR ) {
        uint32_t bytes_written = file_write( wr_file, buf, count );
        return bytes_written;
    } else {
        console_put_str( "sys_write: not allowed to write file without flag O_RDWR or O_WRONLY\n" );
        return -1;
    }
}

// read count bytes from fd file to buf.
int32_t sys_read( int32_t fd, void* buf, uint32_t count ) {
    if ( fd < 0 ) {
        printk( "sys_read: fd error.\n" );
        return -1;
    }
    ASSERT( buf != NULL );
    uint32_t _fd = fd_local2global( fd );
    return file_read( &file_table[ _fd ], buf, count );
}

// get fd file size
int32_t sys_getsize( int32_t fd ) {
    uint32_t _fd = fd_local2global( fd );
    PFILE pf = &file_table[ _fd ];
    int32_t size = ( int32_t )pf->fd_inode->i_size;
    return size;
}

// reset offset pointer of fd
int32_t sys_lseek( int32_t fd, int32_t offset, uint8_t whence ) {
    if ( fd < 0 ) {
        printk( "sys_lseek: fd error\n" );
        return -1;
    }
    ASSERT( whence > 0 && whence < 4 );
    uint32_t _fd = fd_local2global( fd );
    PFILE pf = &file_table[ _fd ];
    int32_t new_pos = 0;
    int32_t file_size = ( int32_t )pf->fd_inode->i_size;  // get file size
    switch ( whence ) {
    // SEEK_SET : file_head + offset
    case SEEK_SET:
        new_pos = offset;
        break;

    // SEEK_CUR : current pos + offset
    case SEEK_CUR:
        new_pos = ( int32_t )pf->fd_pos + offset;
        break;

    // SEEK_END file size + offset
    case SEEK_END:  // offset is negative
        new_pos = file_size + offset;
    }
    if ( new_pos < 0 || new_pos > ( file_size - 1 ) ) {  // new position must in file
        return -1;
    }
    pf->fd_pos = new_pos;
    return pf->fd_pos;
}

// delete file(except dir), return 0 if success, otherwise return -1
int32_t sys_unlink( const char* pathname ) {
    ASSERT( strlen( pathname ) < MAX_PATH_LEN );

    // find file first
    PATH_SEARCH_RECORD searched_record;
    memset( &searched_record, 0, sizeof( PATH_SEARCH_RECORD ) );
    int inode_no = search_file( pathname, &searched_record );
    ASSERT( inode_no != 0 );
    if ( inode_no == -1 ) {
        printk( "file %s not found!\n", pathname );
        dir_close( searched_record.parent_dir );
        return -1;
    }
    if ( searched_record.file_type == FT_DIRECTORY ) {
        printk( "cannot delete a direcotry with unlink(), use rmdir() to instead!\n" );
        dir_close( searched_record.parent_dir );
        return -1;
    }

    // check file_table, cannot delete if used
    uint32_t file_idx = 0;
    while ( file_idx < MAX_FILE_OPEN ) {
        if ( file_table[ file_idx ].fd_inode != NULL && ( uint32_t )inode_no == file_table[ file_idx ].fd_inode->i_number ) {
            break;
        }
        file_idx++;
    }
    if ( file_idx < MAX_FILE_OPEN ) {
        dir_close( searched_record.parent_dir );
        printk( "file %s is in use, not allow to delete!\n", pathname );
        return -1;
    }
    ASSERT( file_idx == MAX_FILE_OPEN );

    // buffer, provide up to 2 sectors size
    void* io_buf = sys_malloc( SECTOR_SIZE + SECTOR_SIZE );
    if ( io_buf == NULL ) {
        dir_close( searched_record.parent_dir );
        printk( "sys_unlink: malloc for io_buf failed\n" );
        return -1;
    }

    PDIR parent_dir = searched_record.parent_dir;
    delete_dir_entry( cur_part, parent_dir, inode_no, io_buf );
    inode_release( cur_part, inode_no );
    sys_free( io_buf );
    dir_close( searched_record.parent_dir );
    return 0;  // success
}

// create directory, return 0 if success, otherwise return -1
int32_t sys_mkdir( const char* pathname ) {
    uint8_t rollback_step = 0;  // roll back state
    void* io_buf = sys_malloc( SECTOR_SIZE * 2 );
    if ( io_buf == NULL ) {
        printk( "sys_mkdir: sys_malloc for io_buf failed\n" );
        return -1;
    }

    PATH_SEARCH_RECORD searched_record;
    memset( &searched_record, 0, sizeof( PATH_SEARCH_RECORD ) );
    int inode_no = -1;
    inode_no = search_file( pathname, &searched_record );
    if ( inode_no != -1 ) {  // if find the same named file or dir, return false
        printk( "sys_mkdir: file or directory %s exist!\n", pathname );
        rollback_step = 1;
        goto rollback;
    } else {
        // find the last directory or the middle path isn't exist
        uint32_t pathname_depth = path_depth_cnt( ( char* )pathname );
        uint32_t path_searched_depth = path_depth_cnt( searched_record.searched_path );
        if ( pathname_depth != path_searched_depth ) {  // mid dir isn't exist
            printk( "sys_mkdir: cannot access %s, subpath %s isn't exist\n", pathname, searched_record.searched_path );
            rollback_step = 1;
            goto rollback;
        }
    }

    PDIR parent_dir = searched_record.parent_dir;
    /* dir name may has the last char '/', better use searched_record.searched_path, which has no '/' */
    char* dirname = strrchr( searched_record.searched_path, '/' ) + 1;

    inode_no = inode_bitmap_alloc( cur_part );
    if ( inode_no == -1 ) {
        printk( "sys_mkdir: allocate inode failed\n" );
        rollback_step = 1;
        goto rollback;
    }

    INODE new_dir_inode;
    inode_init( inode_no, &new_dir_inode );

    uint32_t block_bitmap_idx = 0;  // 用来记录block对应于block_bitmap中的索引
    int32_t block_lba = -1;
    /* 为目录分配一个块,用来写入目录.和.. */
    block_lba = block_bitmap_alloc( cur_part );
    if ( block_lba == -1 ) {
        printk( "sys_mkdir: block_bitmap_alloc for create directory failed\n" );
        rollback_step = 2;
        goto rollback;
    }
    new_dir_inode.i_sectors[ 0 ] = block_lba;
    // sync block
    block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
    ASSERT( block_bitmap_idx != 0 );
    bitmap_sync( cur_part, block_bitmap_idx, BLOCK_BITMAP );

    // write dir entry of . and ..
    memset( io_buf, 0, SECTOR_SIZE * 2 );  // clear io_buf
    PDIR_ENTRY p_de = ( PDIR_ENTRY )io_buf;

    // init . dir
    memcpy( p_de->filename, ".", 1 );
    p_de->i_no = inode_no;
    p_de->f_type = FT_DIRECTORY;

    p_de++;
    // init .. dir
    memcpy( p_de->filename, "..", 2 );
    p_de->i_no = parent_dir->inode->i_number;
    p_de->f_type = FT_DIRECTORY;
    ide_write( cur_part->my_disk, new_dir_inode.i_sectors[ 0 ], io_buf, 1 );

    new_dir_inode.i_size = 2 * cur_part->sb->dir_entry_size;

    // add self dir entry in parent dir entry table
    DIR_ENTRY new_dir_entry;
    memset( &new_dir_entry, 0, sizeof( DIR_ENTRY ) );
    create_dir_entry( dirname, inode_no, FT_DIRECTORY, &new_dir_entry );
    memset( io_buf, 0, SECTOR_SIZE * 2 );                           // clear io_buf
    if ( !sync_dir_entry( parent_dir, &new_dir_entry, io_buf ) ) {  // sync block_bitmap to disk
        printk( "sys_mkdir: sync_dir_entry to disk failed!\n" );
        rollback_step = 2;
        goto rollback;
    }

    // sync parent inode to the disk
    memset( io_buf, 0, SECTOR_SIZE * 2 );
    inode_sync( cur_part, parent_dir->inode, io_buf );

    // sync this new dir inode to the disk
    memset( io_buf, 0, SECTOR_SIZE * 2 );
    inode_sync( cur_part, &new_dir_inode, io_buf );

    // sync inode bitmap to the disk
    bitmap_sync( cur_part, inode_no, INODE_BITMAP );

    sys_free( io_buf );

    // close parent dir
    dir_close( searched_record.parent_dir );
    return 0;

// if fail in one of the above steps, roll back to recover the resource
rollback:
    switch ( rollback_step ) {
    case 2:
        // fail to create new inode, recover inode number in inode bitmap
        bitmap_set( &cur_part->inode_bitmap, inode_no, 0 );
    case 1:
        dir_close( searched_record.parent_dir );
        break;
    }
    sys_free( io_buf );
    return -1;
}

// search file system in the disk
// create file system if not found
void filesys_init( void ) {
    uint8_t channel_no = 0, dev_no, part_idx = 0;

    // suprt block buffer
    PSUPER_BLOCK sb_buf = ( PSUPER_BLOCK )sys_malloc( SECTOR_SIZE );

    if ( sb_buf == NULL ) {
        PANIC( "alloc memory failed!" );
    }
    printk( "searching filesystem.........\n" );
    while ( channel_no < channel_cnt ) {
        dev_no = 0;
        while ( dev_no < 2 ) {
            if ( dev_no == 0 ) {  // skip base hd60M.img
                dev_no++;
                continue;
            }
            PDISK hd = &channels[ channel_no ].devices[ dev_no ];
            PPARTITION part = hd->prim_parts;
            while ( part_idx < 12 ) {   // 4 primary + 8 logic partition
                if ( part_idx == 4 ) {  // logic parts
                    part = hd->logic_parts;
                }

                if ( part->sec_cnt != 0 ) {
                    // exists partition
                    memset( sb_buf, 0, SECTOR_SIZE );

                    // read the super block of the block and judge fs by magic number
                    ide_read( hd, part->start_lba + 1, sb_buf, 1 );
                    // only support my file system magic.
                    if ( sb_buf->magic == FS_MAGIC ) {
                        printk( "%s has filesystem\n", part->name );
                    } else {
                        // if other fs, we format it as no fs
                        printk( "formatting %s's partition %s......\n", hd->name, part->name );
                        partition_format( part );
                    }
                }
                part_idx++;
                part++;  // next partition
            }
            dev_no++;  // next disk
        }
        channel_no++;  // next channel
    }
    sys_free( sb_buf );

    char default_part[ 8 ] = "sdb1";  // default partition name to mount

    // mount the partition
    list_traversal( &partition_list, mount_partition, ( int )default_part );

    open_root_dir( cur_part );

    /* init file tabel */
    uint32_t fd_idx = 0;
    while ( fd_idx < MAX_FILE_OPEN ) {
        file_table[ fd_idx++ ].fd_inode = NULL;
    }
}
