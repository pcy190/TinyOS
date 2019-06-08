#include "ide.h"
#include "console.h"
#include "debug.h"
#include "interrupt.h"
#include "io.h"
#include "list.h"
#include "memory.h"
#include "stdio-kernel.h"
#include "stdio.h"
#include "string.h"
#include "sync.h"
#include "timer.h"

// ports of disk regs
#define reg_data(channel) (channel->port_base + 0)
#define reg_error(channel) (channel->port_base + 1)
#define reg_sect_cnt(channel) (channel->port_base + 2)
#define reg_lba_l(channel) (channel->port_base + 3)
#define reg_lba_m(channel) (channel->port_base + 4)
#define reg_lba_h(channel) (channel->port_base + 5)
#define reg_dev(channel) (channel->port_base + 6)
#define reg_status(channel) (channel->port_base + 7)
#define reg_cmd(channel) (reg_status(channel))
#define reg_alt_status(channel) (channel->port_base + 0x206)
#define reg_ctl(channel) reg_alt_status(channel)

// reg_alt_status reg
#define BIT_STAT_BSY 0x80  // disk busy
#define BIT_STAT_DRDY 0x40 // driver prepared
#define BIT_STAT_DRQ 0x8   // data prepared

// device reg
#define BIT_DEV_MBS 0xa0 // 7th and 5th bit fixed to 1
#define BIT_DEV_LBA 0x40
#define BIT_DEV_DEV 0x10

// disk instruction
#define CMD_IDENTIFY 0xec     // identify instruction
#define CMD_READ_SECTOR 0x20  // read sector instruction
#define CMD_WRITE_SECTOR 0x30 // write sector instruction

// TODO : used to test
// Defined maximum sector number to read/write, only for test (assert lba index
// range)
// now support 80M disk, you can change it
#define max_lba ((80 * 1024 * 1024 / 512) - 1)

uint8_t channel_cnt;     // channel number of all disks
IDE_CHANNEL channels[2]; // two ide channels

// extend partition stated lba, inited by partition_scan
int32_t ext_lba_base = 0;

// disk primary partition and logic partition index
uint8_t p_no = 0, l_no = 0;

// partition list
LIST partition_list;

/*------------------ partition structure define -------------------------------
 */
// partition_table struct (16byte)
typedef struct _PARTITION_TABLE_ENREY {
  uint8_t bootable;       // bootable
  uint8_t start_head;     // start head index
  uint8_t start_sector;   // start sector index
  uint8_t start_cylinder; // start cylinder index
  uint8_t partition_type; // partition(fs) type
  uint8_t end_head;       // end head index
  uint8_t end_sector;     // end sector index
  uint8_t end_cylinder;   // end cylinder index

  uint32_t start_lba;  // started lba of this sector
  uint32_t sector_cnt; // sector number of this sector
} __attribute__((packed)) partition_table_entry, PARTITION_TABLE_ENREY,
    *PPARTITION_TABLE_ENREY; // pack struct to ensure 16byte size

// boot_sector (store mbr/ebr)
typedef struct _BOOT_SECTOR {
  uint8_t other[446];                       // boot code
  PARTITION_TABLE_ENREY partition_table[4]; // four partition_tables, 64bytes
  uint16_t signature;                       // boot sector ends with 0x55,0xaa
} __attribute__((packed)) boot_sector, BOOT_SECTOR, *PBOOT_SECTOR;
/*-----------------------------------------------------------------------------
 */

// select disk to read/write
static void select_disk(PDISK hd) {
  uint8_t reg_device = BIT_DEV_MBS | BIT_DEV_LBA;
  if (hd->dev_no == 1) { // if slave: set DEV bit as 1
    reg_device |= BIT_DEV_DEV;
  }
  // write to device reg
  outb(reg_dev(hd->my_channel), reg_device);
}

// write control reg with started LBA and number of sector
static void select_sector(PDISK hd, uint32_t lba, uint8_t sec_cnt) {
  ASSERT(lba <= max_lba);
  PIDE_CHANNEL channel = hd->my_channel;

  // write sector cnt
  outb(reg_sect_cnt(channel), sec_cnt); // 如果sec_cnt为0,则表示写入256个扇区

  // write lba
  outb(reg_lba_l(channel),
       lba); // lba 0~7bit (outb %b0, %w1 will intercept as 8 bit al)
  outb(reg_lba_m(channel), lba >> 8);  // lba 8~15bit
  outb(reg_lba_h(channel), lba >> 16); // lba 16~23bit

  // lba 24~27 store in device reg 0～3 bit
  // renew the device register
  outb(reg_dev(channel), BIT_DEV_MBS | BIT_DEV_LBA |
                             (hd->dev_no == 1 ? BIT_DEV_DEV : 0) | lba >> 24);
}

// send cmd to channel
static void cmd_out(PIDE_CHANNEL channel, uint8_t cmd) {
  // symbols if send cmd
  // to note the disk intr handler
  channel->expecting_intr = true;
  outb(reg_cmd(channel), cmd);
}

// read sec_cnt sectors data to buf
static void read_from_sector(PDISK hd, void *buf, uint8_t sec_cnt) {
  uint32_t size_in_byte;
  // change sector size to word size
  if (sec_cnt == 0) {
    // because of int overflow, if sec_cnt==0, means 256
    size_in_byte = 256 * 512;
  } else {
    size_in_byte = sec_cnt * 512;
  }
  insw(reg_data(hd->my_channel), buf, size_in_byte / 2);
}

// write sec_cnt sectors data to disk
static void write_to_sector(PDISK hd, void *buf, uint8_t sec_cnt) {
  uint32_t size_in_byte;
  // change sector size to word size
  if (sec_cnt == 0) {
    // because of int overflow, if sec_cnt==0, means 256
    size_in_byte = 256 * 512;
  } else {
    size_in_byte = sec_cnt * 512;
  }
  outsw(reg_data(hd->my_channel), buf, size_in_byte / 2);
}

// wait while disk is busy, maximum 30s totally
static bool busy_wait(PDISK hd) {
  PIDE_CHANNEL channel = hd->my_channel;
  // as ATA book says:All actions required in this state shall be completed
  // within 31 s
  uint16_t time_limit = 30 * 1000; // maximum wait time
  while (time_limit -= 10 >= 0) {
    if (!(inb(reg_status(channel)) & BIT_STAT_BSY)) {
      return (inb(reg_status(channel)) & BIT_STAT_DRQ);
    } else {
      sleep(10);
    }
  }
  return false;
}

// read sec_cnt sectors from disk to buf
void ide_read(PDISK hd, uint32_t lba, void *buf, uint32_t sec_cnt) {
  ASSERT(lba <= max_lba);
  ASSERT(sec_cnt > 0);
  lock_acquire(&hd->my_channel->lock);

  // 1. select disk
  select_disk(hd);

  uint32_t secs_to_read;  // sectors (tmp number) to each read operation
  uint32_t secs_done = 0; // finished sectors
  while (secs_done < sec_cnt) {
    // disk maximum support 256 sectors to operate each time
    if ((secs_done + 256) <= sec_cnt) {
      secs_to_read = 256;
    } else {
      secs_to_read = sec_cnt - secs_done;
    }

    // 2. specify started lba and sector number
    select_sector(hd, lba + secs_done, secs_to_read);

    // 3. out read command
    cmd_out(hd->my_channel, CMD_READ_SECTOR);
    // block when disk works(busy).
    // wake when disk finished read
    sema_down(&hd->my_channel->disk_done);

    // 4. detect disk status (whether ready to read)
    // exec here while wake
    if (!busy_wait(hd)) {
      char error[64];
      sprintf(error, "[ERROR] %s read sector %d FAIL!!!\n", hd->name, lba);
      PANIC(error);
    }

    // 5. read data from disk buf
    read_from_sector(hd, (void *)((uint32_t)buf + secs_done * 512),
                     secs_to_read);
    secs_done += secs_to_read;
  }
  lock_release(&hd->my_channel->lock);
}

// write sec_cnt sectors from buf to disk
void ide_write(PDISK hd, uint32_t lba, void *buf, uint32_t sec_cnt) {
  ASSERT(lba <= max_lba);
  ASSERT(sec_cnt > 0);
  lock_acquire(&hd->my_channel->lock);

  // 1. select disk
  select_disk(hd);

  uint32_t secs_to_read;  // sectors (tmp number) to each read operation
  uint32_t secs_done = 0; // finished sectors
  while (secs_done < sec_cnt) {
    if ((secs_done + 256) <= sec_cnt) {
      secs_to_read = 256;
    } else {
      secs_to_read = sec_cnt - secs_done;
    }

    // 2. specify started lba and sector number
    select_sector(hd, lba + secs_done, secs_to_read);

    // 3. out read command
    cmd_out(hd->my_channel, CMD_WRITE_SECTOR);

    // 4. detect disk status (whether ready to read)
    if (!busy_wait(hd)) {
      char error[64];
      sprintf(error, "[ERROR] %s write sector %d failed!!!!!!\n", hd->name,
              lba);
      PANIC(error);
    }

    // 5. write data to disk
    write_to_sector(hd, (void *)((uint32_t)buf + secs_done * 512),
                    secs_to_read);

    // block while disk working
    sema_down(&hd->my_channel->disk_done);
    secs_done += secs_to_read;
  }
  lock_release(&hd->my_channel->lock);
}

// disk interrupt handler
void disk_intr_handler(uint8_t irq_no) {
  ASSERT(irq_no == 0x2e || irq_no == 0x2f);
  // 0x2e: slave IRQ14 ; 0x2f: slave IRQ15
  uint8_t ch_no = irq_no - 0x2e;
  PIDE_CHANNEL channel = &channels[ch_no];
  ASSERT(channel->irq_no == irq_no);
  // Here we ensure expecting_intr sync to each operation because read/write
  // disk locked each time
  if (channel->expecting_intr) {
    channel->expecting_intr = false;
    sema_up(&channel->disk_done);

    /* To makes disk handler know the interrput is handled, we can:
    1. read status reg
    2. send reset cmd
    3. new command to reg_cmd
    here we choice first method, i.e. read the status reg.
    */
    inb(reg_status(channel));
  }
}

// exchange len adjacent bytes from dst to buf.
static void swap_pairs_bytes(const char *dst, char *buf, uint32_t len) {
  uint8_t idx;
  for (idx = 0; idx < len; idx += 2) {
    // exchange
    buf[idx + 1] = *dst++;
    buf[idx] = *dst++;
  }
  buf[idx] = '\0';
}

// get disk info
static void identify_disk(PDISK hd) {
  char id_info[512];
  select_disk(hd);
  cmd_out(hd->my_channel, CMD_IDENTIFY);
  // wait disk identify work
  sema_down(&hd->my_channel->disk_done);

  // wake
  if (!busy_wait(hd)) {
    char error[64];
    sprintf(error, "[ERROR]%s identify failed!!!!!!\n", hd->name);
    PANIC(error);
  }
  read_from_sector(hd, id_info, 1);

  char buf[64];
  uint8_t sn_start = 10 * 2, sn_len = 20, md_start = 27 * 2, md_len = 40;
  swap_pairs_bytes(&id_info[sn_start], buf, sn_len);
  printk("   disk %s info:\n      SN: %s\n", hd->name, buf);
  memset(buf, 0, sizeof(buf));
  swap_pairs_bytes(&id_info[md_start], buf, md_len);
  printk("      MODULE: %s\n", buf);
  uint32_t sectors = *(uint32_t *)&id_info[60 * 2];
  printk("      SECTORS: %d\n", sectors);
  printk("      CAPACITY: %dMB\n", sectors * 512 / 1024 / 1024);
}

// scan every extend partition(addr: ext_lba) of hd by traversal
static void partition_scan(PDISK hd, uint32_t ext_lba) {
  // avoid stack overflow
  PBOOT_SECTOR bs = sys_malloc(sizeof(BOOT_SECTOR));
  ide_read(hd, ext_lba, bs, 1);
  uint8_t part_idx = 0;
  PPARTITION_TABLE_ENREY p = bs->partition_table;
  // four partition table
  while (part_idx++ < 4) {
    if (p->partition_type == 0x5) {
      // ext partition
      if (ext_lba_base != 0) {
        // sub_ext's start_lba is relative to boot sector's ext_partition
        partition_scan(hd, p->start_lba + ext_lba_base);
      } else {
        // ext_lba_base==0 means read ext lba firstly
        // i.e. boot sector's ext partition
        // record this start_lba as base
        ext_lba_base = p->start_lba;
        partition_scan(hd, p->start_lba);
      }
    } else if (p->partition_type != 0) {
      // valid partition
      if (ext_lba == 0) {
        // primary partition
        hd->prim_parts[p_no].start_lba = ext_lba + p->start_lba;
        hd->prim_parts[p_no].sec_cnt = p->sector_cnt;
        hd->prim_parts[p_no].my_disk = hd;
        list_append(&partition_list, &hd->prim_parts[p_no].part_tag);

        sprintf(hd->prim_parts[p_no].name, "%s%d", hd->name, p_no + 1);
        p_no++;
        ASSERT(p_no < 4); // 0~3 primary part
      } else {
        // logic partition
        hd->logic_parts[l_no].start_lba = ext_lba + p->start_lba;
        hd->logic_parts[l_no].sec_cnt = p->sector_cnt;
        hd->logic_parts[l_no].my_disk = hd;
        list_append(&partition_list, &hd->logic_parts[l_no].part_tag);
        sprintf(hd->logic_parts[l_no].name, "%s%d", hd->name,
                l_no + 5); // logic partition index starts with 5

        l_no++;
        if (l_no >= MAXIMUM_LOGIC_PARTS_NUM) // now support
                                             // MAXIMUM_LOGIC_PARTS_NUM (i.e. 8)
          return;
      }
    }
    p++;
  }
  sys_free(bs);
}

// print partition info
static bool print_partition_info(PLIST_NODE pelem, int arg UNUSED) {
  PPARTITION part = elem2entry(PARTITION, part_tag, pelem);
  //%s start_lba:0x%x, sec_cnt:0x%x\n"
  printk("   %s start_lba:%d, sec_cnt:%d\n", part->name, part->start_lba,
         part->sec_cnt);

  // return false only for list_traversal to traversal next node
  return false;
}

// init disk data structure
void ide_init() {
  printk("ide_init start\n");
  uint8_t hd_cnt = *((uint8_t *)(0x475)); // get the number of the disk
  ASSERT(hd_cnt > 0);
  channel_cnt = DIV_ROUND_UP(
      hd_cnt,
      2); // calc ide channels number because one ide channel contains two disks
  PIDE_CHANNEL channel;
  uint8_t channel_no = 0, dev_no = 0;

  // handle disks of each channel
  while (channel_no < channel_cnt) {
    channel = &channels[channel_no];
    vsprintf(channel->name, "ide%d", channel_no);

    // init base port and interrupt vector number of each ide
    switch (channel_no) {
    case 0:
      channel->port_base = 0x1f0;  // ide0's base port : 0x1f0
      channel->irq_no = 0x20 + 14; // slave 8259a's last but one Interrupt pin,
                                   // i.e. ide0's iqr number
      break;
    case 1:
      channel->port_base = 0x170;  // ide1's base port : 0x170
      channel->irq_no = 0x20 + 15; // slave 8259A's last pin
                                   // used to respond ide1's disk interrupt
      break;
    }

    channel->expecting_intr = false; // If not writing disk, the expect is false
    lock_init(&channel->lock);

    // init as 0 to block this thread until disk prepared and interrupt
    // (which sema_up the semaphore and wake thread)
    sema_init(&channel->disk_done, 0);
    register_handler(channel->irq_no, disk_intr_handler);
    while (dev_no < 2) {
      PDISK hd = &channel->devices[dev_no];
      hd->my_channel = channel;
      hd->dev_no = dev_no;
      sprintf(hd->name, "sd%c", 'a' + channel_no * 2 + dev_no);
      identify_disk(hd);
      if (dev_no != 0) { // we don't scan kernel disk (hd.img)
        partition_scan(hd, 0);
      }
      p_no = 0, l_no = 0;
      dev_no++;
    }
    channel_no++; // next channel
  }
  printk("\n   all partition info\n");
  list_traversal(&partition_list, print_partition_info, NULL);
  printk("ide_init done\n");
}
