#include "print.h"
#include "init.h"
#include "debug.h"
#include "memory.h"
#include "thread.h"
void kernel_thread_function(void*);
int main(){
    put_str("Kernel Started!\n");
    init_all();
    
    thread_start("kernel_thread_main",31,kernel_thread_function,"HAPPY ");
    //asm volatile("sti");
    while(1);
    return 0;
}
void kernel_thread_function(void* arg){
    char * para=arg;
    while(1){
        put_str(para);
    }
    
}