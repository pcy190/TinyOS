#include "fs.h"
#include "debug.h"
#include "dir.h"
#include "global.h"
#include "ide.h"
#include "inode.h"
#include "list.h"
#include "memory.h"
#include "stdint.h"
#include "stdio-kernel.h"
#include "string.h"
#include "super_block.h"

PPARTITION cur_part; // current partition cursor

// find the part_name partition and assign the cur to cur_part
static bool mount_partition(PLIST_NODE pelem, void *arg) {
  char *part_name = (char *)arg;
  PPARTITION part = elem2entry(PARTITION, part_tag, pelem);
  if (!strcmp(part->name, part_name)) {
    cur_part = part;
    struct disk *hd = cur_part->my_disk;

    // malloc sb_buf to store super block from disk
    PSUPER_BLOCK sb_buf = (PSUPER_BLOCK)sys_malloc(SECTOR_SIZE);

    // create super block of cur_part in memory
    cur_part->sb = (PSUPER_BLOCK)sys_malloc(sizeof(SUPER_BLOCK));
    if (cur_part->sb == NULL) {
      PANIC("alloc memory failed!");
    }

    // read super block
    memset(sb_buf, 0, SECTOR_SIZE);
    ide_read(hd, cur_part->start_lba + 1, sb_buf, 1);

    // copy sb_buf super block to partition's super block
    memcpy(cur_part->sb, sb_buf, sizeof(SUPER_BLOCK));

    // read disk's bitmap to memory
    cur_part->block_bitmap.bits =
        (uint8_t *)sys_malloc(sb_buf->block_bitmap_sectors * SECTOR_SIZE);
    if (cur_part->block_bitmap.bits == NULL) {
      PANIC("alloc memory failed!");
    }
    cur_part->block_bitmap.btmp_bytes_len =
        sb_buf->block_bitmap_sectors * SECTOR_SIZE;
    // read bitmap from disk to block_bitmap.bits
    ide_read(hd, sb_buf->block_bitmap_lba, cur_part->block_bitmap.bits,
             sb_buf->block_bitmap_sectors);
    /********************************************************************/

    /**********     read inode bitmap from disk to memory    ************/
    printk("Going to malloc %d  %d\n",sb_buf->inode_bitmap_sectors,sb_buf->inode_bitmap_sectors * SECTOR_SIZE);
    cur_part->inode_bitmap.bits =
        (uint8_t *)sys_malloc(sb_buf->inode_bitmap_sectors * SECTOR_SIZE);
    if (cur_part->inode_bitmap.bits == NULL) {
      PANIC("alloc memory failed!");
    }
    cur_part->inode_bitmap.btmp_bytes_len =
        sb_buf->inode_bitmap_sectors * SECTOR_SIZE;
    // read inode bitmap from disk to inode_bitmap.bits
    ide_read(hd, sb_buf->inode_bitmap_lba, cur_part->inode_bitmap.bits,
             sb_buf->inode_bitmap_sectors);
    /********************************************************************/

    list_init(&cur_part->open_inodes);
    printk("mount %s done!\n", part->name);
    // return true to stop list_traversal because we have coped with the
    // cur_part
    return true;
  }
  return false; // return false to continue list_traversal
}

// format partition, create file system
static void partition_format(PPARTITION part) {
  uint32_t boot_sector_sects = 1;
  uint32_t super_block_sects = 1;
  // bitmap support maximum 4096 files
  uint32_t inode_bitmap_sects =
      DIV_ROUND_UP(MAX_FILES_PER_PART, BITS_PER_SECTOR);
  uint32_t inode_table_sects =
      DIV_ROUND_UP(((sizeof(INODE) * MAX_FILES_PER_PART)), SECTOR_SIZE);
  uint32_t used_sects = boot_sector_sects + super_block_sects +
                        inode_bitmap_sects + inode_table_sects;
  uint32_t free_sects = part->sec_cnt - used_sects;

  /************** calc block bitmap's sectors ***************/
  uint32_t block_bitmap_sectors;
  block_bitmap_sectors = DIV_ROUND_UP(free_sects, BITS_PER_SECTOR);
  // block_bitmap_bit_len: availble blocks number
  uint32_t block_bitmap_bit_len = free_sects - block_bitmap_sectors;
  block_bitmap_sectors = DIV_ROUND_UP(block_bitmap_bit_len, BITS_PER_SECTOR);
  /*********************************************************/

  // init super block
  SUPER_BLOCK sb;
  sb.magic = FS_MAGIC;
  sb.sec_cnt = part->sec_cnt;
  sb.inode_cnt = MAX_FILES_PER_PART;
  sb.part_lba_base = part->start_lba;
  // TODO : lba base offset
  sb.block_bitmap_lba =
      sb.part_lba_base + 2; // 0th block: boot block, 1st block: super block
  sb.block_bitmap_sectors = block_bitmap_sectors;

  sb.inode_bitmap_lba = sb.block_bitmap_lba + sb.block_bitmap_sectors;
  sb.inode_bitmap_sectors = inode_bitmap_sects;

  sb.inode_table_lba = sb.inode_bitmap_lba + sb.inode_bitmap_sectors;
  sb.inode_table_sectors = inode_table_sects;

  sb.data_start_lba = sb.inode_table_lba + sb.inode_table_sectors;
  sb.root_inode_no = 0;
  sb.dir_entry_size = sizeof(struct dir_entry);

  printk("%s info:\n", part->name);
  printk("   magic:0x%x\n   part_lba_base:0x%x\n   all_sectors:0x%x\n   "
         "inode_cnt:0x%x\n   block_bitmap_lba:0x%x\n   "
         "block_bitmap_sectors:0x%x\n   inode_bitmap_lba:0x%x\n   "
         "inode_bitmap_sectors:0x%x\n   inode_table_lba:0x%x\n   "
         "inode_table_sectors:0x%x\n   data_start_lba:0x%x\n",
         sb.magic, sb.part_lba_base, sb.sec_cnt, sb.inode_cnt,
         sb.block_bitmap_lba, sb.block_bitmap_sectors, sb.inode_bitmap_lba,
         sb.inode_bitmap_sectors, sb.inode_table_lba, sb.inode_table_sectors,
         sb.data_start_lba);

  PDISK hd = part->my_disk;
  /*******************************
   * 1 write super block to 1th sectors *
   ******************************/
  ide_write(hd, part->start_lba + 1, &sb, 1);
  printk("   super_block_lba:0x%x\n", part->start_lba + 1);

  // get buf size
  uint32_t buf_size = (sb.block_bitmap_sectors >= sb.inode_bitmap_sectors
                           ? sb.block_bitmap_sectors
                           : sb.inode_bitmap_sectors);
  buf_size =
      (buf_size >= sb.inode_table_sectors ? buf_size : sb.inode_table_sectors) *
      SECTOR_SIZE;
  uint8_t *buf = (uint8_t *)sys_malloc(buf_size); // zero set memory

  /**************************************
   * 2 init block bitmap and write to sb.block_bitmap_lba *
   *************************************/
  // init block_bitmap
  buf[0] |= 0x01; // 0th block reserved to / (root dir)
  uint32_t block_bitmap_last_byte = block_bitmap_bit_len / 8;
  uint8_t block_bitmap_last_bit = block_bitmap_bit_len % 8;
  uint32_t last_size =
      SECTOR_SIZE - (block_bitmap_last_byte %
                     SECTOR_SIZE); // last_size: the remain size of last sector

  // 1 we declare the part which out of the block bitmap range is used
  // (set bit from last bitmap to sector end as 1)
  memset(&buf[block_bitmap_last_byte], 0xff, last_size);

  // 2 reset bits in last byte as 0
  uint8_t bit_idx = 0;
  while (bit_idx <= block_bitmap_last_bit) {
    buf[block_bitmap_last_byte] &= ~(1 << bit_idx++);
  }
  ide_write(hd, sb.block_bitmap_lba, buf, sb.block_bitmap_sectors);

  /***************************************
   * 3 init inode bitmap and write to sb.inode_bitmap_lba *
   ***************************************/
  memset(buf, 0, buf_size);
  buf[0] |= 0x1; // 0th inode: used by root dir
  /* 4096 inode in inode_table. inode_bitmap occupy 1 sector
     i.e. inode_bitmap_sectors==1
     so we don't need to deal the more used bit like block_bitmap
   */
  ide_write(hd, sb.inode_bitmap_lba, buf, sb.inode_bitmap_sectors);

  /***************************************
   * 4 init inode array and write to sb.inode_table_lba *
   ***************************************/
  /* write inode_table 0th table (root dir)*/
  // set buf as 0 and ensure inode is empty
  memset(buf, 0, buf_size);
  PINODE i = (PINODE)buf;
  i->i_size = sb.dir_entry_size * 2;   // . and .. dir
  i->i_number = 0;                     // root dir is the 0th inode
  i->i_sectors[0] = sb.data_start_lba; // 0th data block
  ide_write(hd, sb.inode_table_lba, buf, sb.inode_table_sectors);

  /***************************************
   * 5 init root dir and write to sb.data_start_lba
   ***************************************/
  // write . and ..
  memset(buf, 0, buf_size);
  struct dir_entry *p_de = (struct dir_entry *)buf;

  // init . dir
  memcpy(p_de->filename, ".", 1);
  p_de->i_no = 0;
  p_de->f_type = FT_DIRECTORY;
  p_de++;

  // init .. dir
  memcpy(p_de->filename, "..", 2);
  p_de->i_no = 0; // root's parent dir is root
  p_de->f_type = FT_DIRECTORY;

  // sb.data_start_lba contains root dir tables
  ide_write(hd, sb.data_start_lba, buf, 1);

  printk("   root_dir_lba:0x%x\n", sb.data_start_lba);
  printk("%s format done\n", part->name);
  sys_free(buf);
}

// search file system in the disk
// create file system if not found
void filesys_init() {
  uint8_t channel_no = 0, dev_no, part_idx = 0;

  // suprt block buffer
  PSUPER_BLOCK sb_buf = (PSUPER_BLOCK)sys_malloc(SECTOR_SIZE);

  if (sb_buf == NULL) {
    PANIC("alloc memory failed!");
  }
  printk("searching filesystem.........\n");
  while (channel_no < channel_cnt) {
    dev_no = 0;
    while (dev_no < 2) {
      if (dev_no == 0) { // skip base hd60M.img
        dev_no++;
        continue;
      }
      PDISK hd = &channels[channel_no].devices[dev_no];
      PPARTITION part = hd->prim_parts;
      while (part_idx < 12) { // 4 primary + 8 logic partition
        if (part_idx == 4) {  // logic parts
          part = hd->logic_parts;
        }

        if (part->sec_cnt != 0) {
          // exists partition
          memset(sb_buf, 0, SECTOR_SIZE);

          // read the super block of the block and judge fs by magic number
          ide_read(hd, part->start_lba + 1, sb_buf, 1);

          // only support my file system magic.
          if (sb_buf->magic == FS_MAGIC) {
            printk("%s has filesystem\n", part->name);
          } else {
            // if other fs, we format it as no fs
            printk("formatting %s's partition %s......\n", hd->name,
                   part->name);
            partition_format(part);
          }
        }
        part_idx++;
        part++; // next partition
      }
      dev_no++; // next disk
    }
    channel_no++; // next channel
  }
  sys_free(sb_buf);

  char default_part[8] = "sdb1"; // partition name to mount

  // mount the partition
  list_traversal(&partition_list, mount_partition, (int)default_part);
}
