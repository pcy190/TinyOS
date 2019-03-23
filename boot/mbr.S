;mbr
;Author: HAPPY
;----------------------------------------------
%include "include/boot.inc"
SECTION MBR vstart=0x7c00         

    mov ax,cs
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov fs,ax
    mov sp,0x7c00
    mov ax,0xb800
    mov gs,ax


;Clear screen

;use int 10h which ah=6  (function number: 0x06)
;----------------------------------------------
;INPUT:
;al: line number to clear
;bh: line attribute
;(CL,CH) Top left point
;(DL,DH) Lower right pointer
;no return
    mov ax,0600h
    mov bx,0700h
    xor cx,cx
    mov dx,184fh

    int 10h


;Print string
    	mov ecx,(String_end-String)
  	xor bx,bx
	xor si,si
	push ds
	mov ds,si
print_loop:
    mov al,[ds:String+si]
    mov byte [gs:bx],al
    inc bx
    mov byte [gs:bx],0x2

		;attribute
		;7   6 5 4  3  2 1 0
		;BL  R G B  I  R G B
		;   ------     ----- 
		;Twinkle background Highlight foreground

    inc bx
    inc si
    loop print_loop
	pop ds


;-------------
    mov eax,LOADER_START_SECTOR     ;loader start sector number
    mov bx,LOADER_BASE_ADDR         ;target memory addr to write
    mov cx,4                        ;sector number to read

    call rd_disk
    jmp LOADER_BASE_ADDR

;-------------

;Read sector from disk function

;INPUT:
;eax: LBA sector index
;ebx: target memory addr to write
;ecx: sector number to read

rd_disk:
    ;backup register
    mov esi,eax
    mov di,cx
    
;first step: set sector number
    mov dx,0x1f2
    mov al,cl
    out dx,al   ;sector number to read
    
    mov eax,esi

;second step: Output LBA addr to 0x1f3 ~ 0x1f6

    ;output LBA 0~7 bit to port 0x1f3
    mov dx,0x1f3
    out dx,al

    ;output LBA 8~15 bit to port 0x1f4
    mov cl,8
    shr eax,cl
    mov dx,0x1f4
    out dx,al

    ;output LBA 16~23 bit to port 0x1f5
    shr eax,cl
    mov dx,0x1f5
    out dx,al

    shr eax,cl
    and al,0x0F     ;LBA 24~27 bit 
    or al,0xe0      ;set 4~7 bit 1110, indicating LBA mode
    mov dx,0x1f6
    out dx,al

;third step: output port 0x1f7 with 0x20, indicating write command
    mov dx,0x1f7
    mov al,0x20
    out dx,al

;fourth step: monitor disk state

.not_ready:
    nop
    in al,dx
    and al,0x88  ;disk is ready when 4th bit is 1 ,busy with 7th bit is 1
    cmp al,0x08
    jnz .not_ready

;fifth step: read data from 0x1f0
    mov ax,di
    mov dx,256
    mul dx
    mov cx,ax   ;di is sector number to read, each time read 1 word
                ;so di*512/2 times totally
    mov dx, 0x1f0

.go_on_read:
    in ax,dx
    mov [bx],ax
    add bx,2
    loop .go_on_read
    ret 


String:
     db "MBR started"
String_end:

times 510-($-$$) db 0
db 0x55,0xaa
