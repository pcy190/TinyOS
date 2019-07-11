#include "shell.h"
#include "assert.h"
#include "buildin_cmd.h"
#include "file.h"
#include "fs.h"
#include "global.h"
#include "stdint.h"
#include "stdio.h"
#include "string.h"
#include "syscall.h"


#define cmd_len 128        // maximum cmd char length
#define MAX_ARG_NUMBER 16  // cmd +15 argv

char* argv[ MAX_ARG_NUMBER ];

// input cmd buffer
static char cmd_line[ cmd_len ] = {0};
char final_path[ MAX_PATH_LEN ] = {0};  // path buffer

// current work directry. maintained by "cd"
char cwd_cache[ 64 ] = {0};

// prompt
void print_prompt( void ) { printf( "[HAPPY@localhost %s]$ ", cwd_cache ); }

// read up to count bytes to buf from keyboard_buffer
static void readline( char* buf, int32_t count ) {
    assert( buf != NULL && count > 0 );
    char* pos = buf;
    while ( read( FD_STDIN, pos, 1 ) != -1 && ( pos - buf ) < count ) {  // read until \n enter char
        switch ( *pos ) {
            // face enter char, stop read
        case '\n':
        case '\r':
            *pos = 0;  // append str 0
            putchar( '\n' );
            return;

        case '\b':
            if ( buf[ 0 ] != '\b' ) {  // avoid delete chars before buf.
                --pos;                 // backward pos
                putchar( '\b' );
            }
            break;
        case 'l' - 'a':  // shortcuts: ctrl+l : clear screen but keep the current cmd
            *pos = 0;
            clear();
            print_prompt();
            // reshow the current cmd
            printf( "%s", buf );
            break;

        case 'u' - 'a':  // ctrl+u clear current input line
            while ( buf != pos ) {
                putchar( '\b' );
                *( pos-- ) = 0;
            }
            break;

        default:
            putchar( *pos );
            pos++;
        }
    }
    printf( "Readline: cannot find enter_key in the cmd_line, max number of input cmd char is %d\n", cmd_len );
}

// parse cmd_str with token ( split word ), save pointers to argv table
static int32_t cmd_parse( char* cmd_str, char** argv, char token ) {
    assert( cmd_str != NULL );
    int32_t arg_idx = 0;
    while ( arg_idx < MAX_ARG_NUMBER ) {
        argv[ arg_idx ] = NULL;
        arg_idx++;
    }
    char* next = cmd_str;
    int32_t argc = 0;
    while ( *next ) {
        // strip token
        while ( *next == token ) {
            next++;
        }
        if ( *next == 0 ) {
            break;
        }
        argv[ argc ] = next;

        // find next token pos
        while ( *next && *next != token ) {
            next++;
        }

        // set token as 0 to close the last word string
        if ( *next ) {
            *next++ = 0;
        }

        // arg number judge
        if ( argc > MAX_ARG_NUMBER ) {
            return -1;
        }
        argc++;
    }
    return argc;
}

int32_t argc = -1;

void my_shell( void ) {
    cwd_cache[ 0 ] = '/';
    while ( 1 ) {
        print_prompt();
        memset( cmd_line, 0, cmd_len );
        memset( final_path, 0, MAX_PATH_LEN );
        readline( cmd_line, cmd_len );
        if ( cmd_line[ 0 ] == 0 ) {  // empty cmd
            continue;
        }
        argc = -1;
        argc = cmd_parse( cmd_line, argv, ' ' );
        if ( argc == -1 ) {
            printf( "num of arguments exceed %d\n", MAX_ARG_NUMBER );
            continue;
        }

        char buf[ MAX_PATH_LEN ] = {0};
        int32_t arg_idx = 0;
        while ( arg_idx < argc ) {
            make_clear_abs_path( argv[ arg_idx ], buf );
            printf( "%s ---> %s", argv[ arg_idx ], buf );
            arg_idx++;
        }
        printf( "\n" );
    }
    panic( "my_shell: should not be here. This is end func\n" );
}
