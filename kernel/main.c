#include "print.h"
#include "init.h"
int main(){
    put_char('k');
    put_char('e');
    put_char('r');
    put_char('n');
    put_char('e');
    put_char('l');
    put_char('\n');
    put_str("Powered by HAPPY!\n");
    init_all();
    asm volatile("sti");
    while(1);
    return 0;
}