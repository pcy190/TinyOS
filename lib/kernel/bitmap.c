#include "bitmap.h"
#include "debug.h"
#include "interrupt.h"
#include "print.h"
#include "stdint.h"
#include "string.h"

// init bitmap
void bitmap_init( struct bitmap* btmp ) { memset( btmp->bits, 0, btmp->btmp_bytes_len ); }

// return true if the bit_idx bit is 1, otherwise return false
bool bitmap_scan_test( struct bitmap* btmp, uint32_t bit_idx ) {
    uint32_t byte_idx = bit_idx / 8;  // table index
    uint32_t bit_odd = bit_idx % 8;   // left bit
    return ( btmp->bits[ byte_idx ] & ( BITMAP_MASK << bit_odd ) );
}

// alloc continuous cnt bits in bitmap
// return start index if success
// return -1 if fail
int bitmap_scan( struct bitmap* btmp, uint32_t cnt ) {
    uint32_t idx_byte = 0;
    while ( ( 0xff == btmp->bits[ idx_byte ] ) && ( idx_byte < btmp->btmp_bytes_len ) ) {
        // 0xff means this bytes is full, skip it
        idx_byte++;
    }

    ASSERT( idx_byte < btmp->btmp_bytes_len );
    if ( idx_byte == btmp->btmp_bytes_len ) {  // no spare space
        return -1;
    }

    int idx_bit = 0;
    // compare each bit
    while ( ( uint8_t )( BITMAP_MASK << idx_bit ) & btmp->bits[ idx_byte ] ) {
        idx_bit++;
    }

    int bit_idx_start = idx_byte * 8 + idx_bit;  // slot bit index
    if ( cnt == 1 ) {
        return bit_idx_start;
    }

    uint32_t bit_left = ( btmp->btmp_bytes_len * 8 - bit_idx_start );
    uint32_t next_bit = bit_idx_start + 1;
    uint32_t count = 1;  // slot bit which has been found

    bit_idx_start = -1;
    while ( bit_left-- > 0 ) {
        if ( !( bitmap_scan_test( btmp, next_bit ) ) ) {  // next_bit is 0, search next
            count++;
        } else {
            count = 0;
        }
        if ( count == cnt ) {  // found cnt slot bits
            bit_idx_start = next_bit - cnt + 1;
            break;
        }
        next_bit++;
    }
    return bit_idx_start;
}

// set bit_idx bit in bitmap as value
void bitmap_set( struct bitmap* btmp, uint32_t bit_idx, int8_t value ) {
    ASSERT( ( value == 0 ) || ( value == 1 ) );
    uint32_t byte_idx = bit_idx / 8;
    uint32_t bit_odd = bit_idx % 8;

    if ( value ) {  // 1
        btmp->bits[ byte_idx ] |= ( BITMAP_MASK << bit_odd );
    } else {
        btmp->bits[ byte_idx ] &= ~( BITMAP_MASK << bit_odd );
    }
}
