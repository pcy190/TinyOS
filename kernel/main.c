#include "console.h"
#include "debug.h"
#include "dir.h"
#include "fs.h"
#include "init.h"
#include "interrupt.h"
#include "ioqueue.h"
#include "memory.h"
#include "process.h"
#include "stdio.h"
#include "string.h"
#include "syscall-init.h"
#include "syscall.h"
#include "thread.h"

void init( void );

int main() {
    put_str( "Kernel Started!\n" );
    init_all();

    while ( 1 ) {
        // console_put_str("Main ");
    }
    return 0;
}

void init( void ) {
    printf( "I am init process\n" );
    uint32_t ret_pid = fork();
    printf( "This is first sentence with %d\n", ret_pid );
    printf( "This is second sentence with %d\n", ret_pid );
    if(ret_pid) {
       printf("i am father, my pid is %d, child pid is %d\n", getpid(), ret_pid);
    } else {
        printf( "Child comes\n" );
        printf( "i am child, my pid is %d, ret pid is %d\n", getpid(), ret_pid );
    }
    while ( 1 )
        ;
}