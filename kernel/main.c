#include "console.h"
#include "debug.h"
#include "init.h"
#include "interrupt.h"
#include "ioqueue.h"
#include "memory.h"
#include "process.h"
#include "thread.h"

void kernel_thread_function(void *);
void kernel_thread_functionB(void *arg);
void u_prog_a(void);
void u_prog_b(void);
int test_var_a = 0, test_var_b = 0;
int main() {
  put_str("Kernel Started!\n");
  init_all();
  thread_start("k_thread_a", 31, kernel_thread_function, "argA ");
  thread_start("k_thread_b", 31, kernel_thread_functionB, "argB ");
  process_execute(u_prog_a, "user_prog_a");
  process_execute(u_prog_b, "user_prog_b");

  // thread_start("kernel_thread_main", 31, kernel_thread_function, "H ");
  // thread_start("kernel_thread_mainA", 31, kernel_thread_function, "A ");
  // thread_start("kernel_thraed_B", 31, kernel_thread_functionB, "arg B ");
  // asm volatile("sti");
  intr_enable();
  while (1) {
    // console_put_str("Main ");
  }
  return 0;
}
void kernel_thread_function(void *arg) {
  char *para = arg;
  while (1) {
    console_put_str(" v_a:0x");
    console_put_int(test_var_a);
  }
}
void kernel_thread_functionB(void *arg) {
  char *para = arg;
  while (1) {
    console_put_str(" v_b:0x");
    console_put_int(test_var_b);
  }
}
void u_prog_a(void) {
  while (1) {
    test_var_a++;
  }
  return;
}

void u_prog_b(void) {
  while (1) {
    test_var_b++;
  }
}
