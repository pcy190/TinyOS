#ifndef __KERNEL_INTERRUPT_H
#define __KERNEL_INTERRUPT_H
#include "stdint.h"
typedef void* intr_handler;
void idt_init(void);

/* Define interrupt status
 * INTR_OFF:0 close interrupt
 * INTR_ON :1 open interrupt */
typedef enum _INTR_STATUS {		 
    INTR_OFF,			 
    INTR_ON		         
}INTR_STATUS;

INTR_STATUS intr_get_status(void);
INTR_STATUS intr_set_status (INTR_STATUS intr_status);
INTR_STATUS intr_enable (void);
INTR_STATUS intr_disable (void);

#endif
