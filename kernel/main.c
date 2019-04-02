#include "init.h"
#include "debug.h"
#include "memory.h"
#include "thread.h"
#include "interrupt.h"
#include "console.h"
#include "ioqueue.h"

void kernel_thread_function(void *);
void kernel_thread_functionB(void *arg);
int main()
{
    put_str("Kernel Started!\n");
    init_all();

    //thread_start("kernel_thread_main", 31, kernel_thread_function, "H ");
    //thread_start("kernel_thread_mainA", 31, kernel_thread_function, "A ");
    //thread_start("kernel_thraed_B", 31, kernel_thread_functionB, "arg B ");
    //asm volatile("sti");
    intr_enable();
    while (1)
    {
        //console_put_str("Main ");        
    }
    return 0;
}
void kernel_thread_function(void *arg)
{
    char *para = arg;
    while (1)
    {
        console_put_str(para);
    }
}
void kernel_thread_functionB(void *arg)
{
    char *para = arg;
    while (1)
    {
        console_put_str(para);
    }
}