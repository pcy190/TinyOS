#ifndef __FS_FILE_H
#define __FS_FILE_H
#include "dir.h"
#include "global.h"
#include "ide.h"
#include "stdint.h"

typedef struct _FILE {
    uint32_t fd_pos;   // offset starts with 0
    uint32_t fd_flag;  // operate flag
    PINODE fd_inode;
} FILE, *PFILE;

// standard file descriptor
typedef enum _STD_FD {
    FD_STDIN,   // 0 stdin
    FD_STDOUT,  // 1 stdout
    FD_STDERR,  // 2 stderr
} STD_FD;

// bitmap type
typedef enum _BITMAP_TYPE {
    INODE_BITMAP,  // inode bitmap
    BLOCK_BITMAP   // block bitmap
} BITMAP_TYPE;

#define MAX_FILE_OPEN 32  // max file open times

extern FILE file_table[ MAX_FILE_OPEN ];
int32_t inode_bitmap_alloc( PPARTITION part );
int32_t block_bitmap_alloc( PPARTITION part );
int32_t file_create( struct _DIR* parent_dir, char* filename, uint8_t flag );
int32_t file_read( PFILE file, void* buf, uint32_t count );
void bitmap_sync( PPARTITION part, uint32_t bit_idx, uint8_t btmp );
int32_t get_free_slot_in_global( void );
int32_t pcb_fd_install( int32_t globa_fd_idx );
#endif
