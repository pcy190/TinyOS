TI_GDT equ  0
RPL0  equ   0
SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL0

section .data

buffer dq 0

[bits 32]
section .text


; ----------------- put_char ---------------------
;input : char in stack 
;write one char in the stack to the cursor pos
global put_char
put_char:
    pushad
    mov ax, SELECTOR_VIDEO
    mov gs,ax

    ; get cursor position

    mov dx, 0x03d4           ; get cursor pos high 8bit
    mov al, 0x0e
    out dx, al
    mov dx, 0x03d5
    in al, dx
    mov ah,al

    mov dx, 0x03d4           ; get cursor pos low 8bit
    mov al, 0x0f
    out dx, al
    mov dx, 0x03d5
    in al, dx

    mov bx,ax

    mov ecx, [esp+36]        ; pushad needs 4*8=32byte, ret addr needs 4byte
    cmp cl, 0xd              ; CR: 0x0d
    jz .char_carriage_return
    cmp cl, 0xa              ; LF: 0x0xa
   jz .char_line_feed

   cmp cl, 0x8               ; backspace: 8  (ascii)
   jz .char_backspace
   jmp .put_other
   ; ;;;;;;;;;;;;;;;;;

 .char_backspace:
   ; backspace cursor , and put space char on the cursor pos
   dec bx
   shl bx,1
   mov byte [gs:bx], 0x20    ; put space char on the cursor pos, to make it looks like backspace
   inc bx
   mov byte [gs:bx], 0x07
   shr bx,1
   jmp .set_cursor
   ; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

 .put_other:
   shl bx, 1                 ; tmp real VIDEO addr
   mov [gs:bx], cl           ; ascii
   inc bx
   mov byte [gs:bx],0x07     ; attribute
   shr bx, 1                 ; recover cursor pos
   inc bx                    ; next cursor
   cmp bx, 2000
   jl .set_cursor            ; less than 2000 means current page has spare space
                             ; more than 2000, need LF
 .char_line_feed:            ; LF(\n)
 .char_carriage_return:      ; CR(\r)    ; deal \n\r ==> \n
   xor dx, dx
   mov ax, bx
   mov si, 80
   div si
   sub bx, dx                ; Rounding
                             ; deal \r char

 .char_carriage_return_end:
   add bx, 80
   cmp bx, 2000
 .char_line_feed_end:        ; LF(\n), cursor pos+80。
   jl .set_cursor


 .roll_screen:
   cld
   mov ecx, 960              ; move 2000-80=1920 char , 1920*2=3840 byte. each time 4 byte, 3840/4=960 times
   mov esi, 0xb80a0          ; line 1 pos
   mov edi, 0xb8000          ; line 0 pos
   rep movsd

   ; Blank the last line
   mov ebx, 3840             ; last line first char pos: 1920 * 2
   mov ecx, 80               ; 80 times
 .cls:
   mov word [gs:ebx], 0x0720 ; 0x0720: space char with white font(foreground), black background
   add ebx, 2
   loop .cls
   mov bx,1920               ; reset cursor to the last line first pos

 .set_cursor:
    ; set cursor as BX value
    ; set high 8 bit
    mov dx, 0x03d4
    mov al, 0x0e
    out dx, al
    mov dx, 0x03d5            ; write port 0x3d5 to get/write cursor pos
    mov al, bh
    out dx, al

    ;set low 8 bit  
    mov dx, 0x03d4
    mov al, 0x0f
    out dx, al
    mov dx, 0x03d5
    mov al, bl
    out dx, al
    
    ;putchar done
    ;recover 
    popad
    ret






;------------------ putint ----------------------
;input : number in stack 
;print HEX number
global putint
putint:
