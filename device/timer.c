#include "timer.h"
#include "io.h"
#include "print.h"

// IRQ0_FREQUENCY : IRQ0 (clock intr) frequency 
// notice COUNTER0_VALUE<65535.  When COUNTER0_VALUE==0,means 65536
#define IRQ0_FREQUENCY	   100          
#define INPUT_FREQUENCY	   1193180
#define COUNTER0_VALUE	   (INPUT_FREQUENCY / IRQ0_FREQUENCY)
#define CONTRER0_PORT	   0x40
#define COUNTER0_NO	   0
#define COUNTER_MODE	   2
#define READ_WRITE_LATCH   3
#define PIT_CONTROL_PORT   0x43

/* init CONTROL reg with counter_no,read write lock:rwl,counter_mode,counter_value */
static void frequency_set(uint8_t counter_port, 
			  uint8_t counter_no, 
			  uint8_t rwl, 
			  uint8_t counter_mode, 
			  uint16_t counter_value) {
/* write to control port 0x43 */
   outb(PIT_CONTROL_PORT, (uint8_t)(counter_no << 6 | rwl << 4 | counter_mode << 1));
/* counter_value low 8bit */
   outb(counter_port, (uint8_t)counter_value);
/* counter_value high 8bit */
   outb(counter_port, (uint8_t)counter_value >> 8);
}

/* 初始化PIT8253 */
void timer_init() {
   put_str("timer init start\n");
   /* set 8253 intrrupt interval */
   frequency_set(CONTRER0_PORT, COUNTER0_NO, READ_WRITE_LATCH, COUNTER_MODE, COUNTER0_VALUE);
   put_str("timer init done\n");
}