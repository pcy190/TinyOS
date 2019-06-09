#ifndef __FS_DIR_H
#define __FS_DIR_H
#include "global.h"
#include "ide.h"
#include "inode.h"
#include "stdint.h"
#include "fs.h"


// file name maximum length
#define MAX_FILE_NAME_LEN 16

// directory structure (used only in memory)
struct dir {
  PINODE inode;
  uint32_t dir_pos;     // offset in dir
  uint8_t dir_buf[512]; // dir data buffer
};

// dir entry structure
struct dir_entry {
  char filename[MAX_FILE_NAME_LEN]; // name
  uint32_t i_no;                    // inode number
  FILE_TYPES f_type;           // file type
};

#endif // !__FS_DIR_H
