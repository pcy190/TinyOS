#ifndef __DEVICE_IDE_H
#define __DEVICE_IDE_H
#include "stdint.h"
#include "sync.h"
#include "bitmap.h"

//partition structure
typedef struct _PARTITION {
   uint32_t start_lba;		 // Start Sector
   uint32_t sec_cnt;		 // Sector Number
   struct disk* my_disk;	 // disk which contains this partition
   struct list_elem part_tag;	 // tag of queue
   char name[8];		 // partition name
   struct super_block* sb;	 // partition's super_block
   struct bitmap block_bitmap;	 // block_bitmap
   struct bitmap inode_bitmap;	 // inode_bitmap
   LIST open_inodes;	 // open_inodes of this partition
}partition,PARTITION;

//disk structure
typedef struct _DISK {
   char name[8];			   // disk name
   struct ide_channel* my_channel;	   // ide channel of this disk
   uint8_t dev_no;			   // master:0 ; slave:1
   struct partition prim_parts[4];	   // primary support up to 4 parts
   struct partition logic_parts[8];	   // logic support up to 8 parts (You can alter it)
}disk,DISK;

//ata channel's structure
struct ide_channel {
   char name[8];		 // ata channel's name 
   uint16_t port_base;		 // base port of this channel
   uint8_t irq_no;		 // interrupt number of this channel
   LOCK lock;		 // channel lock
   bool expecting_intr;		 // interrupt of waiting
   SEMAPHORE disk_done;	 // semaphore to sleep/wake driver
   struct disk devices[2];	 // master&slave disk
};

void ide_init(void);
extern uint8_t channel_cnt;
extern struct ide_channel channels[];
#endif
