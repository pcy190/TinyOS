#include "console.h"
#include "debug.h"
#include "dir.h"
#include "fs.h"
#include "init.h"
#include "interrupt.h"
#include "ioqueue.h"
#include "memory.h"
#include "print.h"
#include "process.h"
#include "shell.h"
#include "stdio.h"
#include "string.h"
#include "syscall-init.h"
#include "syscall.h"
#include "thread.h"


void init( void );

int main() {
    put_str( "Kernel Started!\n" );
    init_all();
    cls_screen();
    print_prompt();
    while ( 1 ) {
        // console_put_str("Main ");
    }
    return 0;
}

void init( void ) {
    uint32_t ret_pid = fork();
    if ( ret_pid ) {
        while ( 1 )
            ;
    } else {
        my_shell();
    }
    PANIC( "init: shouldn't be here\n" );
}