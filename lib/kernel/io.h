/**************** machine mode ********************
 *  b : QImode ,Half Integer, register low 8bit , e.g. [a-d]l
 *  w : HImode Quarter Integer, register 2 byte , e.g. [a-d]x
 **************************************************/

#ifndef __LIB_IO_H
#define __LIB_IO_H
#include "stdint.h"

/* write one byte to port */
static inline void outb(uint16_t port, uint8_t data){

    /* 
        a means ise al, ax or eax
        N means appoint port 0~255
        d means use dx store port number
        %b0: al
        %w1: dx
     */
    asm volatile("outb %b0, %w1"
    :
    :"a"(data),"Nd" (port)
    );

}


// write to port
// write word_cnt word from addr to port 
static inline void outsw(uint16_t port,const void *addr,uint32_t word_cnt){
    /*  
        + means both input and output
        outsw asm write ds:esi 16bit to port
        */
    asm volatile ("cld; rep outsw" 
    : "+S" (addr), "+c" (word_cnt) 
    : "d" (port)
    );

}

// read from port
/* write word_cnt word from port to addr */
static inline void insw(uint16_t port, void* addr, uint32_t word_cnt) {
    //insw asm write 16bit from port to es:edi mem
   asm volatile ("cld; rep insw" 
   : "+D" (addr), "+c" (word_cnt) 
   : "d" (port) : "memory"
   );

}

// read from port
/* write word_cnt byte from port to addr */
static inline uint8_t inb(uint16_t port) {
   uint8_t data;
   asm volatile ("inb %w1, %b0" : "=a" (data) : "Nd" (port));
   return data;
}

#endif