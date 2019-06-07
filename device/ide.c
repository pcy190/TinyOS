#include "ide.h"
#include "debug.h"
#include "interrupt.h"
#include "memory.h"
#include "stdio-kernel.h"
#include "stdio.h"
#include "string.h"
#include "sync.h"

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
// Defined maximum sector number to read/write, only for test
#define max_lba ((80 * 1024 * 1024 / 512) - 1) // now support 80 M disk

uint8_t channel_cnt;            // channel number of all disks
struct ide_channel channels[2]; // two ide channels

// init disk data structure
void ide_init() {
  printk("ide_init start\n");
  uint8_t hd_cnt = *((uint8_t *)(0x475)); // get the number of the disk
  ASSERT(hd_cnt > 0);
  channel_cnt = DIV_ROUND_UP(
      hd_cnt,
      2); // calc ide channels number because one ide channel contains two disks
  struct ide_channel *channel;
  uint8_t channel_no = 0;

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
    channel_no++; // next channel
  }
  printk("ide_init done\n");
}
