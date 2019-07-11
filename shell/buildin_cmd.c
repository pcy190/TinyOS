#include "buildin_cmd.h"
#include "assert.h"
#include "dir.h"
#include "fs.h"
#include "global.h"
#include "shell.h"
#include "stdio.h"
#include "string.h"
#include "syscall.h"


// TODO : buf length check
// parse .. and . of old_abs_path to real_path, and store to new_abs_path
static void wash_path( char* old_abs_path, char* new_abs_path ) {
    assert( old_abs_path[ 0 ] == '/' );
    char name[ MAX_FILE_NAME_LEN ] = {0};
    char* sub_path = old_abs_path;
    sub_path = path_parse( sub_path, name );
    if ( name[ 0 ] == 0 ) {  // only / dir
        new_abs_path[ 0 ] = '/';
        new_abs_path[ 1 ] = 0;
        return;
    }
    new_abs_path[ 0 ] = 0;  // clear buf
    strcat( new_abs_path, "/" );
    while ( name[ 0 ] ) {
        // .. dir
        if ( !strcmp( "..", name ) ) {
            char* slash_ptr = strrchr( new_abs_path, '/' );
            if ( slash_ptr != new_abs_path ) {
                *slash_ptr = 0;
            } else {
                // reach top / dir , add 0 to str end
                *( slash_ptr + 1 ) = 0;
            }
        } else if ( strcmp( ".", name ) ) {       // strcat current name to path
            if ( strcmp( new_abs_path, "/" ) ) {  // avoid // case
                strcat( new_abs_path, "/" );
            }
            strcat( new_abs_path, name );
        }

        memset( name, 0, MAX_FILE_NAME_LEN );
        if ( sub_path ) {
            sub_path = path_parse( sub_path, name );
        }
    }
}

// parse to absolute path without . or .. , store to final_path
// TODO : path buf length ensure MAX_PATH_LEN
void make_clear_abs_path( char* path, char* final_path ) {
    char abs_path[ MAX_PATH_LEN ] = {0};
    // judge whether abs path
    if ( path[ 0 ] != '/' ) {
    // strcat to make abs path
        memset( abs_path, 0, MAX_PATH_LEN );
        if ( getcwd( abs_path, MAX_PATH_LEN ) != NULL ) {
            if ( !( ( abs_path[ 0 ] == '/' ) && ( abs_path[ 1 ] == 0 ) ) ) {  // not root dir
                strcat( abs_path, "/" );
            }
        }
    }
    strcat( abs_path, path );
    wash_path( abs_path, final_path );
}
