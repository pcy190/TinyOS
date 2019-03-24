#include "print.h"
#include "init.h"
#include "interrupt.h"
#include "timer.h"

//INIT all 
void init_all(){
    put_str("start init all\n");
    idt_init();
    timer_init();
    put_str("finish init all\n");
}