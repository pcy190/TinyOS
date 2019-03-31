#include "console.h"
#include "print.h"
#include "stdint.h"
#include "sync.h"
#include "thread.h"
static struct _LOCK console_lock;   //console lock

/* init console lock */
void console_init() {
  lock_init(&console_lock); 
}

/* get console lock */
void console_acquire() {
   lock_acquire(&console_lock);
}

/* release console lock */
void console_release() {
   lock_release(&console_lock);
}

void console_put_str(char* str) {
   console_acquire(); 
   put_str(str); 
   console_release();
}

void console_put_char(uint8_t char_asci) {
   console_acquire(); 
   put_char(char_asci); 
   console_release();
}

void console_put_int(uint32_t num) {
   console_acquire(); 
   put_int(num); 
   console_release();
}

