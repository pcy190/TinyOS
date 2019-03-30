#include "print.h"
#include "init.h"
#include "debug.h"
#include "memory.h"
int main(){
    put_str("Kernel Started!\n");
    init_all();
    uint32_t va=(uint32_t*) malloc_kernel_pages(3);
    if (va!=NULL) {
        put_str("Malloc Kernel Pages: ");
        put_int(va);
        put_char('\n');
    }else{
        put_str("malloc failed!\n");
    }
    
    //asm volatile("sti");
    while(1);
    return 0;
}