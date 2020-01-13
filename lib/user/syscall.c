#include "syscall.h"
#include "thread.h"

// syscall without argc
#define _syscall0( NUMBER )                                                                                                                                                                            \
    ( {                                                                                                                                                                                                \
        int retval;                                                                                                                                                                                    \
        asm volatile( "int $0x80" : "=a"( retval ) : "a"( NUMBER ) : "memory" );                                                                                                                       \
        retval;                                                                                                                                                                                        \
    } )

// syscall with 1 argc
#define _syscall1( NUMBER, ARG1 )                                                                                                                                                                      \
    ( {                                                                                                                                                                                                \
        int retval;                                                                                                                                                                                    \
        asm volatile( "int $0x80" : "=a"( retval ) : "a"( NUMBER ), "b"( ARG1 ) : "memory" );                                                                                                          \
        retval;                                                                                                                                                                                        \
    } )

// syscall with 2 argc
#define _syscall2( NUMBER, ARG1, ARG2 )                                                                                                                                                                \
    ( {                                                                                                                                                                                                \
        int retval;                                                                                                                                                                                    \
        asm volatile( "int $0x80" : "=a"( retval ) : "a"( NUMBER ), "b"( ARG1 ), "c"( ARG2 ) : "memory" );                                                                                             \
        retval;                                                                                                                                                                                        \
    } )

// syscall with 3 argc
#define _syscall3( NUMBER, ARG1, ARG2, ARG3 )                                                                                                                                                          \
    ( {                                                                                                                                                                                                \
        int retval;                                                                                                                                                                                    \
        asm volatile( "int $0x80" : "=a"( retval ) : "a"( NUMBER ), "b"( ARG1 ), "c"( ARG2 ), "d"( ARG3 ) : "memory" );                                                                                \
        retval;                                                                                                                                                                                        \
    } )

// return pid of current task
uint32_t getpid() { return _syscall0( SYS_GETPID ); }

int32_t read( int32_t fd, void* buf, uint32_t count ) { return _syscall3( SYS_READ, fd, buf, count ); }

uint32_t write( int32_t fd, const void* buf, uint32_t count ) { return _syscall3( SYS_WRITE, fd, buf, count ); }

void* malloc( uint32_t size ) { return ( void* )_syscall1( SYS_MALLOC, size ); }

void free( void* ptr ) { _syscall1( SYS_FREE, ptr ); }

pid_t fork( void ) { return _syscall0( SYS_FORK ); }

void putchar( char target_char ) { _syscall1( SYS_PUTCHAR, target_char ); }

void clear( void ) { _syscall0( SYS_CLEAR ); }

char* getcwd( char* buf, uint32_t size ) { return ( char* )_syscall2( SYS_GETCWD, buf, size ); }

int32_t open( char* pathname, uint8_t flag ) { return _syscall2( SYS_OPEN, pathname, flag ); }

int32_t close( int32_t fd ) { return _syscall1( SYS_CLOSE, fd ); }

int32_t lseek( int32_t fd, int32_t offset, uint8_t whence ) { return _syscall3( SYS_LSEEK, fd, offset, whence ); }

int32_t unlink( const char* pathname ) { return _syscall1( SYS_UNLINK, pathname ); }

int32_t mkdir( const char* pathname ) { return _syscall1( SYS_MKDIR, pathname ); }

PDIR opendir( const char* name ) { return ( PDIR )_syscall1( SYS_OPENDIR, name ); }

int32_t closedir( PDIR dir ) { return _syscall1( SYS_CLOSEDIR, dir ); }

int32_t rmdir( const char* pathname ) { return _syscall1( SYS_RMDIR, pathname ); }

PDIR_ENTRY readdir( PDIR dir ) { return ( PDIR_ENTRY )_syscall1( SYS_READDIR, dir ); }

// reset dir pointer
void rewinddir( PDIR dir ) { _syscall1( SYS_REWINDDIR, dir ); }

// get path file attribute
int32_t stat( const char* path, PSTAT buf ) { return _syscall2( SYS_STAT, path, buf ); }

int32_t chdir( const char* path ) { return _syscall1( SYS_CHDIR, path ); }

void ps( void ) { _syscall0( SYS_PS ); }
