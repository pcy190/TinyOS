#ifndef __LIB_USER_SYSCALL_H
#define __LIB_USER_SYSCALL_H
#include "stdint.h"
typedef enum _SYSCALL_NR {
   SYS_GETPID
}SYSCALL_NR;
uint32_t getpid(void);
#endif

