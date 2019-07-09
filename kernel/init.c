#include "print.h"
#include "init.h"
#include "interrupt.h"
#include "timer.h"
#include "memory.h"
#include "thread.h"
#include "console.h"
#include "keyboard.h"
#include "tss.h"
#include "syscall-init.h"
#include "ide.h"
#include "fs.h"

//INIT all 
void init_all(){
    put_str("start init all\n");
    idt_init();
    mem_init();
    thread_init();
    timer_init();
    console_init();
    keyboard_init();
    tss_init();
    syscall_init();
    intr_enable(); //ide_init need intr
    ide_init();
    //put_str("finish init all\n");
    filesys_init();
}