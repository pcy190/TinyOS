#ifndef __DEVICE_IOQUEUE_H
#define __DEVICE_IOQUEUE_H
#include "stdint.h"
#include "thread.h"
#include "sync.h"

#define bufsize 64

/* Annular queue */
typedef struct _IOQUEUE
{
  LOCK lock;

  //producer sleep when the ioqueue is full.
  //record here so we can wake it while ioqueue is not full
  PTASK_STRUCT producer;

  //consumer sleep when the ioqueue is empty.
  //record here so we can wake it while ioqueue is not empty
  PTASK_STRUCT consumer;
  char buf[bufsize]; 
  int32_t head;      // data write starts with head
  int32_t tail;      // data read starts with tail
}IOQUEUE,*PIOQUEUE;

void ioqueue_init(PIOQUEUE ioq);
bool ioq_full(PIOQUEUE ioq);
char ioq_getchar(PIOQUEUE ioq);    
void ioq_putchar(PIOQUEUE ioq, char byte);
#endif
