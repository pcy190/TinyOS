#include "stdio.h"
#include "debug.h"
#include "global.h"
#include "interrupt.h"
#include "print.h"
#include "string.h"
#include "syscall.h"

//DELETE
#include"console.h"

#define va_start(ap, v) ap = (va_list)&v // make ap points to the first V
#define va_arg(ap, t) *((t *)(ap += 4))  // make ap points to next arg
#define va_end(ap) ap = NULL             // clear ap

// integer to ascii
static void itoa(uint32_t value, char **buf_ptr_addr, uint8_t base) {
  uint32_t m = value % base;
  uint32_t i = value / base;
  if (i) { // recursion
    itoa(i, buf_ptr_addr, base);
  }
  if (m < 10) {
    *((*buf_ptr_addr)++) = m + '0'; //'0'~'9'
  } else {
    *((*buf_ptr_addr)++) = m - 10 + 'A'; //'A'~'F'
  }
}

// output format string to str
// return length
uint32_t vsprintf(char *str, const char *format, va_list ap) {
  char *buf_ptr = str;
  const char *index_ptr = format;
  char index_char = *index_ptr;
  int32_t arg_int;
  char* arg_str;
  while (index_char) {
    //Fixed \n problem
    if (index_char=='\n')
    {
      *(buf_ptr++) = '\n';
      //while(1);
      index_ptr++;
      //write("I find the enter at\n");
      //break;
      index_char = (*index_ptr);
      continue;
    }
    
    if (index_char != '%') {
      *(buf_ptr++) = index_char;
      index_char = *(++index_ptr);
      continue;
    }
    index_char = *(++index_ptr); // get symbols of %
    switch (index_char) {
    case 's':
      arg_str = va_arg(ap, char *);
      strcpy(buf_ptr, arg_str);
      buf_ptr += strlen(arg_str);
      index_char = *(++index_ptr);
      break;

    case 'c':
      *(buf_ptr++) = va_arg(ap, char);
      index_char = *(++index_ptr);
      break;

    case 'd':
      arg_int = va_arg(ap, int);
      //negative
      if (arg_int < 0) {
        arg_int = 0 - arg_int;
        *buf_ptr++ = '-';
      }
      itoa(arg_int, &buf_ptr, 10);
      index_char = *(++index_ptr);
      break;
    case 'x':
      arg_int = va_arg(ap, int);
      itoa(arg_int, &buf_ptr, 16);
      index_char = *(++index_ptr); // skip %x
      break;
    default:
      break;
    }
  }
  return strlen(str);
}

#define LENGTH 1024
// char buf[1024] = {0};   // buffer, note that we should check the length
uint32_t printf(const char *format, ...) {}
uint32_t printf_(const char *format, ...) {
  // write("IN PRINTF");
  // write("IN PRINTFs");

  va_list args;
  va_start(args, format); // make args points to format
                          // TODO : check the length
  char buf[1024] = {97,20,97,0};   //vsprintf buffer
  //itoa(buf,&buf,10);
  //write('n');
  //write(buf);
  vsprintf(buf, format, args);
  ASSERT(buf[1023]==0);
  va_end(args);
  return write(buf);
  // write("IN PRINTF0\n");
  // vsprintf(buf, format, args);
  // write("IN PRINTF1\n");
  // va_end(args);
  // ASSERT(buf[LENGTH-1]=='\0');
  // return write(buf);
}