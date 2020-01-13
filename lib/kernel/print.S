TI_GDT equ  0
RPL0  equ   0
SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL0

section .data

buffer dq 0

[bits 32]
section .text

; ----------------- put_str ----------------------
; input : zero-string in the stack
; print string
global put_str
put_str:
    push ebx
    push ecx
    xor ecx,ecx
    mov ebx, [esp + 12]
.parse_char:
    mov cl, [ebx]
    cmp cl,0                    ; 0 symbols the end of str
    jz .str_over
    push ecx
    call put_char
    add esp,4
    inc ebx
    jmp .parse_char
.str_over:
    pop ecx
    pop ebx
    ret


    ; ----------------- put_char ---------------------
    ; input : char in stack
    ; write one char in the stack to the cursor pos
global put_char
put_char:
    pushad
    mov ax, SELECTOR_VIDEO
    mov gs,ax

    ; get cursor position

    mov dx, 0x03d4              ; get cursor pos high 8bit
    mov al, 0x0e
    out dx, al
    mov dx, 0x03d5
    in al, dx
    mov ah,al

    mov dx, 0x03d4              ; get cursor pos low 8bit
    mov al, 0x0f
    out dx, al
    mov dx, 0x03d5
    in al, dx

    mov bx,ax

    mov ecx, [esp+36]           ; pushad needs 4*8=32byte, ret addr needs 4byte
    cmp cl, 0xd                 ; CR: 0x0d
    jz .char_carriage_return
    cmp cl, 0xa                 ; LF: 0x0xa
   jz .char_line_feed

   cmp cl, 0x8                  ; backspace: 8  (ascii)
   jz .char_backspace
   jmp .put_other
   ; ;;;;;;;;;;;;;;;;;

 .char_backspace:
   ; backspace cursor , and put space char on the cursor pos
   dec bx
   shl bx,1
   mov byte [gs:bx], 0x20       ; put space char on the cursor pos, to make it looks like backspace
   inc bx
   mov byte [gs:bx], 0x07
   shr bx,1
   jmp .set_cursor
   ; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

 .put_other:
   shl bx, 1                    ; tmp real VIDEO addr
   mov [gs:bx], cl              ; ascii
   inc bx
   mov byte [gs:bx],0x07        ; attribute
   shr bx, 1                    ; recover cursor pos
   inc bx                       ; next cursor
   cmp bx, 2000
   jl .set_cursor               ; less than 2000 means current page has spare space
                                ; more than 2000, need LF
 .char_line_feed:               ; LF(\n)
 .char_carriage_return:         ; CR(\r)    ; deal \n\r ==> \n
   xor dx, dx
   mov ax, bx
   mov si, 80
   div si
   sub bx, dx                   ; Rounding
                                ; deal \r char

 .char_carriage_return_end:
   add bx, 80
   cmp bx, 2000
 .char_line_feed_end:           ; LF(\n), cursor pos+80。
   jl .set_cursor


 .roll_screen:
   cld
   mov ecx, 960                 ; move 2000-80=1920 char , 1920*2=3840 byte. each time 4 byte, 3840/4=960 times
   mov esi, 0xc00b80a0          ; line 1 pos ;FIX Addr problem
   mov edi, 0xc00b8000          ; line 0 pos
   rep movsd

   ; Blank the last line
   mov ebx, 3840                ; last line first char pos: 1920 * 2
   mov ecx, 80                  ; 80 times
 .cls:
   mov word [gs:ebx], 0x0720    ; 0x0720: space char with white font(foreground), black background
   add ebx, 2
   loop .cls
   mov bx,1920                  ; reset cursor to the last line first pos

 .set_cursor:
    ; set cursor as BX value
    ; set high 8 bit
    mov dx, 0x03d4
    mov al, 0x0e
    out dx, al
    mov dx, 0x03d5              ; write port 0x3d5 to get/write cursor pos
    mov al, bh
    out dx, al

    ; set low 8 bit
    mov dx, 0x03d4
    mov al, 0x0f
    out dx, al
    mov dx, 0x03d5
    mov al, bl
    out dx, al

    ; putchar done
    ; recover
    popad
    ret


global cls_screen
cls_screen:
   pushad
   ; user cpl is 3, VIDEO dpl is 0
   ; when user process into the intr, the gs was set as 0, here asign the gs
   mov ax, SELECTOR_VIDEO
   mov gs, ax

   mov ebx, 0
   mov ecx, 80*25
 .cls:
   mov word [gs:ebx], 0x0720    ; 0x0720: white foreground, black background, blank char
   add ebx, 2
   loop .cls
   mov ebx, 0

 .set_cursor:
   ; ;;;;;; 1 set high 8 bits ;;;;;;;;
   mov dx, 0x03d4               ; reg index
   mov al, 0x0e                 ; high 8 bit of cursor position
   out dx, al
   mov dx, 0x03d5               ; r/w 0x3d5 port to get/set cursor pos
   mov al, bh
   out dx, al

   ; ;;;;;; 2 set low 8 bits ;;;;;;;;;
   mov dx, 0x03d4
   mov al, 0x0f
   out dx, al
   mov dx, 0x03d5
   mov al, bl
   out dx, al
   popad
   ret


   ; ------------------ put_int ----------------------
   ; input : number in stack
   ; print HEX format number without prefix "0X"
global put_int
put_int:
    pushad
    mov ebp,esp
    mov eax, [ebp+4*9]
    mov edx,eax

    mov edi,7
    mov ebx,buffer
    mov ecx,8

.num2char:
    and edx,0xF
    cmp edx,9
    jg .num_A2F
    add edx, '0'
    jmp .continue
.num_A2F:
    sub edx,10
    add edx, 'A'

.continue:
    mov [ebx+edi], dl
    dec edi
    shr eax, 4
    mov edx, eax
    loop .num2char

.print_hex_char:
    xor edi,edi                 ; original edi is -1 , set it 0
    xor ecx,ecx
.skip_prefix_0:
    cmp edi,8
    je .full0

.go_on_skip:
    mov cl,[buffer+edi]
    inc edi
    cmp cl, '0'
    je .skip_prefix_0
    dec edi
    jmp .print_all_num

.full0:
    mov cl,'0'

.print_all_num:
    push ecx
    call put_char
    add esp,4
    inc edi
    mov cl,[buffer+edi]
    cmp edi,8
    jl .print_all_num
    popad
    ret

global set_cursor
set_cursor:
  pushad
 mov bx, [esp+36]
   ; --------------  high 8 bit --------------
   mov dx, 0x03d4
   mov al, 0x0e                 ; high 8 bit
   out dx, al
   mov dx, 0x03d5               ; R/W port 0x3d5 to gain/set cursor position
   mov al, bh
   out dx, al

   ; --------------  low 8 bit --------------
   mov dx, 0x03d4
   mov al, 0x0f
   out dx, al
   mov dx, 0x03d5
   mov al, bl
   out dx, al
  popad
  ret