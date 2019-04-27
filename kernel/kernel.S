[bits 32]
%define ERROR_CODE nop
%define ZERO push 0

extern put_str
extern idt_table

section .data

intrrupt_str db "interrupt occur!\n",0

global intr_entry_table

intr_entry_table:

%macro VECTOR 2
section .text
intr%1entry:
    %2
    push ds
    push es
    push fs
    push gs
    pushad
                   ; If intr from slave, we should send EOI to both master and slave
    mov al,0x20    ; EOI command
    out 0xa0,al    ; send to slave
    out 0x20,al    ; send to master

    push %1         ;push intr number 
    call [idt_table + %1*4]       ; call dispatch functions in idt_table (defined in interrupt C file)
    jmp intr_exit
section .data
    dd intr%1entry ; store each interrupt entry
%endmacro



section .text
global intr_exit
intr_exit:	     
; recover
   add esp, 4			   ; skip intr number 
   popad
   pop gs
   pop fs
   pop es
   pop ds
   add esp, 4			   ; skip error_code
   iretd

VECTOR 0x00,ZERO
VECTOR 0x01,ZERO
VECTOR 0x02,ZERO
VECTOR 0x03,ZERO
VECTOR 0x04,ZERO
VECTOR 0x05,ZERO
VECTOR 0x06,ZERO
VECTOR 0x07,ZERO
VECTOR 0x08,ERROR_CODE
VECTOR 0x09,ZERO
VECTOR 0x0a,ERROR_CODE
VECTOR 0x0b,ERROR_CODE
VECTOR 0x0c,ZERO
VECTOR 0x0d,ERROR_CODE
VECTOR 0x0e,ERROR_CODE
VECTOR 0x0f,ZERO
VECTOR 0x10,ZERO
VECTOR 0x11,ERROR_CODE
VECTOR 0x12,ZERO
VECTOR 0x13,ZERO
VECTOR 0x14,ZERO
VECTOR 0x15,ZERO
VECTOR 0x16,ZERO
VECTOR 0x17,ZERO
VECTOR 0x18,ERROR_CODE
VECTOR 0x19,ZERO
VECTOR 0x1a,ERROR_CODE
VECTOR 0x1b,ERROR_CODE
VECTOR 0x1c,ZERO
VECTOR 0x1d,ERROR_CODE
VECTOR 0x1e,ERROR_CODE
VECTOR 0x1f,ZERO
VECTOR 0x20,ZERO    ;clock interrupt 
VECTOR 0x21,ZERO	;keyboard interrupt
VECTOR 0x22,ZERO	;
VECTOR 0x23,ZERO	;com2 entry
VECTOR 0x24,ZERO	;com1 entry
VECTOR 0x25,ZERO	;parallel port2 entry
VECTOR 0x26,ZERO	;floppy entry
VECTOR 0x27,ZERO	;parallel port1 entry
VECTOR 0x28,ZERO	;real clock interrupt
VECTOR 0x29,ZERO	;redirected
VECTOR 0x2a,ZERO	;reserved
VECTOR 0x2b,ZERO	;reserved
VECTOR 0x2c,ZERO	;ps/2 mouse
VECTOR 0x2d,ZERO	;fpu exception
VECTOR 0x2e,ZERO	;harddisk
VECTOR 0x2f,ZERO	;reserved