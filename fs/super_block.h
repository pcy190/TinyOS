#ifndef __FS_SUPER_BLOCK_H
#define __FS_SUPER_BLOCK_H
#include "stdint.h"

//super block
typedef struct _SUPER_BLOCK{
    uint32_t magic;                 // identify file system type
    uint32_t sec_cnt;               // total sectors of this partition
    uint32_t inode_cnt;             // inode number of this partition 
    uint32_t part_lba_base;         // lba base of this partition

    uint32_t block_bitmap_lba;      // block bitmap start lba
    uint32_t block_bitmap_sectors;  // block bitmap occupied sectors size
    
    uint32_t inode_bitmap_lba;      // inode bitmap start lba
    uint32_t inode_bitmap_sectors;  // inode bitmap occupied sectors size

    uint32_t inode_table_lba;	    // inode table start lba
    uint32_t inode_table_sectors;	    // inode table occupied sectors size

    uint32_t data_start_lba;	    // data start lba
    uint32_t root_inode_no;	        // root dir inode number
    uint32_t dir_entry_size;	    // dir entry size

    uint8_t padding[460];           // align to one sector size (4*13+460=512)
} __attribute__ ((packed)) SUPER_BLOCK,*PSUPER_BLOCK;



#endif // !SUPER_BLOCK