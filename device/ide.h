#ifndef __DEVICE_IDE_H
#define __DEVICE_IDE_H
#include "bitmap.h"
#include "list.h"
#include "stdint.h"
#include "super_block.h"
#include "sync.h"


// partition structure
typedef struct _PARTITION {
    uint32_t start_lba;          // Start Sector
    uint32_t sec_cnt;            // Sector Number
    struct disk* my_disk;        // disk which contains this partition
    LIST_NODE part_tag;          // tag of queue
    char name[ 8 ];              // partition name
    PSUPER_BLOCK sb;             // partition's super_block
    struct bitmap block_bitmap;  // block_bitmap
    struct bitmap inode_bitmap;  // inode_bitmap
    LIST open_inodes;            // open_inodes of this partition
} partition, PARTITION, *PPARTITION;

// disk structure
#define MAXIMUM_LOGIC_PARTS_NUM 8
typedef struct _DISK {
    char name[ 8 ];                                    // disk name
    struct _IDE_CHANNEL* my_channel;                   // ide channel of this disk
    uint8_t dev_no;                                    // master:0 ; slave:1
    PARTITION prim_parts[ 4 ];                         // primary support up to 4 parts
    PARTITION logic_parts[ MAXIMUM_LOGIC_PARTS_NUM ];  // logic support up to 8 parts (You can alter it)
} disk, DISK, *PDISK;

// ata channel's structure
typedef struct _IDE_CHANNEL {
    char name[ 8 ];       // ata channel's name
    uint16_t port_base;   // base port of this channel
    uint8_t irq_no;       // interrupt number of this channel
    LOCK lock;            // channel lock
    bool expecting_intr;  // interrupt of waiting
    SEMAPHORE disk_done;  // semaphore to sleep/wake driver
    DISK devices[ 2 ];    // master&slave disk
} ide_channel, IDE_CHANNEL, *PIDE_CHANNEL;

void ide_init( void );
void intr_hd_handler( uint8_t irq_no );
extern uint8_t channel_cnt;
extern IDE_CHANNEL channels[];
extern LIST partition_list;
void ide_read( PDISK hd, uint32_t lba, void* buf, uint32_t sec_cnt );
void ide_write( PDISK hd, uint32_t lba, void* buf, uint32_t sec_cnt );
#endif
