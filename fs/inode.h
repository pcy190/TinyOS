#ifndef __FS_INODE_H
#define __FS_INODE_H
#include "list.h"
#include "ide.h"
#include "stdint.h"

typedef struct _INODE {
    uint32_t i_number;  // inode number

    // if inode is file, i_size : file size
    // if inode is directory, i_size : sizeof all directory entries
    uint32_t i_size;

    uint32_t i_open_cnts;  // file opend count
    bool write_deny;       // write flag. ensure write sync

    // TODO : temporarily support primary indirect block
    // i_sectors[0-11]: direct block;
    // i_sectors[12]: indirect block;
    uint32_t i_sectors[ 13 ];  // here we define 1 block equals 1 sector
                               // i.e i_block in ext2f
    LIST_NODE inode_tag;       // to support inode cache

} INODE, *PINODE;

PINODE inode_open(PPARTITION part, uint32_t inode_no);
void inode_sync(PPARTITION part, PINODE inode, void* io_buf);
void inode_init(uint32_t inode_no, PINODE new_inode);
void inode_close(PINODE inode);

#endif  // __FS_INODE_H
