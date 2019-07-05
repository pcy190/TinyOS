#ifndef __FS_DIR_H
#define __FS_DIR_H
#include "fs.h"
#include "global.h"
#include "ide.h"
#include "inode.h"
#include "stdint.h"

// file name maximum length
#define MAX_FILE_NAME_LEN 16

// directory structure (used only in memory)
typedef struct _DIR {
    PINODE inode;
    uint32_t dir_pos;        // offset in dir
    uint8_t dir_buf[ 512 ];  // dir data buffer
} DIR, *PDIR;

// dir entry structure
typedef struct _DIR_ENTRY {
    char filename[ MAX_FILE_NAME_LEN ];  // name
    uint32_t i_no;                       // inode number
    FILE_TYPES f_type;                   // file type
} DIR_ENTRY, *PDIR_ENTRY;

extern DIR root_dir;             // root dir
void open_root_dir(PPARTITION part);
PDIR dir_open( PPARTITION part, uint32_t inode_no );
void dir_close( PDIR dir );
bool search_dir_entry(PPARTITION part, PDIR pdir, const char* name, PDIR_ENTRY dir_e);
void create_dir_entry(char* filename, uint32_t inode_no, uint8_t file_type, PDIR_ENTRY p_de);
bool sync_dir_entry(PDIR parent_dir, PDIR_ENTRY p_de, void* io_buf);
bool delete_dir_entry( PPARTITION part, PDIR pdir, uint32_t inode_no, void* io_buf );
#endif  // __FS_DIR_H
