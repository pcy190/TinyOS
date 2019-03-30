%include"include/boot.inc"
section loader vstart=LOADER_BASE_ADDR
; build GDT info & other info
GDT_BASE:
    dd 0x00000000
    dd 0x00000000
CODE_DESC:
    dd    0x0000FFFF
    dd    DESC_CODE_HIGH4

DATA_STACK_DESC:
    dd    0x0000FFFF
    dd    DESC_DATA_HIGH4

VIDEO_DESC:
    dd    0x80000007                          ; limit=(0xbffff-0xb8000)/4k=0x7
    dd    DESC_VIDEO_HIGH4                    ; dpl is 0

GDT_SIZE   equ   $ - GDT_BASE
GDT_LIMIT   equ   GDT_SIZE -	1
times 60 dq 0                                 ; remain sixty descriptor table slots
SELECTOR_CODE equ (0x0001<<3) + TI_GDT + RPL0 ; CODE_DESC index + TI_GDT + RPL0
SELECTOR_DATA equ (0x0002<<3) + TI_GDT + RPL0
SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL0

    ; totally 256 bit
    ; here addr: 0x900+0x200=0xb00  (VA)       
total_mem_bytes dd 0					 ; memory  capacity ,based on byte

gdt_ptr
    dw GDT_LIMIT
    dd GDT_BASE


      
ards_buf times 244 db 0
ards_num dw 0                                 ; reocrd ards number



loader_start:
    ; =======================================================
    ; Print string before Protect mode
    mov ecx,(String_end-String)
    xor ebx,ebx
    mov bx,0xA0
    xor si,si
    push ds
    mov ds,si
print_loop:
    mov al,[ds:String+si]
    mov byte [gs:bx],al
    inc bx
    mov byte [gs:bx],0x2

    ; attribute
    ; 7   6 5 4  3  2 1 0
    ; BL  R G B  I  R G B
    ; ------     -----
    ; Twinkle background Highlight foreground

    inc bx
    inc si
    loop print_loop
    pop ds
    ;---------------------------------------------------------
    ;set_cursor:
    ; set cursor as BX value
    ; set high 8 bit
    mov bx, 240
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
    ;--------------------------------------------------------

    ; ========================================================
    ; Probe_memory
    ; ------------  int 15h eax = 0000E820h ,edx = 534D4150h ('SMAP')   ------------
    xor ebx, ebx                              ; ebx: first call with 0
       mov edx, 0x534d4150                    ; edx： signature
       mov di, ards_buf                       ; ards structure buffer
.e820_mem_get_loop:                           ; parse each ARDS
     mov eax, 0x0000e820                      ; update function number because each call changes eax with SMAP 0X534D4150
     mov ecx, 20                              ; ARDS size is 20 byte
     int 0x15
     jc .e820_failed_so_try_e801              ; CF==1 means error ,so we try 0xe801 bios function
     add di, cx                               ; add di with 20 to point di to new ARDS
     inc word [ards_num]                      ; record ARDS number
     cmp ebx, 0                               ; if ebx==0&&cf!=1 means it is the last ARDS
     jnz .e820_mem_get_loop

     ; from all ARDS, find the maximum of (base_add_low + length_low), which equals the capacity of mem。
   mov cx, [ards_num]                         ; ARDS number
   mov ebx, ards_buf
   xor edx, edx                               ; edx means max mem , init it with 0
.find_max_mem_area:
   mov eax, [ebx]                             ; base_add_low
   add eax, [ebx+8]                           ; length_low
   add ebx, 20                                ; next ARDS
   cmp edx, eax
   jge .next_ards
   mov edx, eax
.next_ards:
   loop .find_max_mem_area
   jmp .mem_get_ok

   ; -----------  int 15h ax = E801h ----------
.e820_failed_so_try_e801:
    mov ax,0xe801
       int 0x15
       jc .e801_failed_so_try88

       ; 1 calc 0~15M mem, ax and cx based on KB
   mov cx,0x400                               ; convert cx and ax to byte
   mul cx
   shl edx,16
   and eax,0x0000FFFF
   or edx,eax
   add edx, 0x100000                          ; edx plus 1MB mem
   mov esi,edx                                ; backup

                                              ; 2 calc 16MB~ mem ,bx and dx based on 64KB
   xor eax,eax
   mov ax,bx
   mov ecx, 0x10000                           ; 64KB
   mul ecx                                    ; save high 32bit to edx,low 32 bit to eax.
   add esi,eax                                ; only 4GB can detected, so only use eax
   mov edx,esi                                ; edx: total mem
   jmp .mem_get_ok

   ; -----------------  int 15h ah = 0x88 get mem less than 64MB  ----------
.e801_failed_so_try88:
   mov  ah, 0x88
   int  0x15
   jc .error_hlt
   and eax,0x0000FFFF
   mov cx, 0x400                              ; change kb to byte
   mul cx
   shl edx, 16
   or edx, eax                                ; 把积的低16位组合到edx,为32位的积
   add edx,0x100000                           ; 0x88 function onlt return size of mem larger than 1MB,so add 1MB additionally

.mem_get_ok:
   mov [total_mem_bytes], edx                 ; 将内存换为byte单位后存入total_mem_bytes处。







; Enable and Enter Protect Mode
; ----------Open A20----------
in al,0x92
or al,00000010B
out 0x92,al

; ----------Load GDT----------
lgdt [gdt_ptr]

; ----------set CR0-----------
mov eax,cr0
or eax,0x00000001
mov cr0,eax

jmp SELECTOR_CODE:protect_mode_start          ; flush pipeline (in order to update segment sector)

.error_hlt:                                   ; halt if error
    hlt

[bits 32]
protect_mode_start:
    mov ax,SELECTOR_DATA
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov esp,LOADER_STACK_TOP
    mov ax,SELECTOR_VIDEO
    mov gs,ax

    ; ---------------------
    ; Print msg in Protect Mode
    mov byte [gs:320],'P'
    mov byte [gs:322],'r'
    mov byte [gs:324],'o'
    mov byte [gs:326],'t'
    mov byte [gs:328],'e'
    mov byte [gs:330],'c'
    mov byte [gs:332],'t'
    mov byte [gs:334],''
    mov byte [gs:336],'m'
    mov byte [gs:338],'o'
    mov byte [gs:340],'d'
    mov byte [gs:342],'e'


; =================================== load kernel ====================================
;=====================================================================================

mov eax, KERNEL_START_SECTOR                  ; sector number
mov ebx, KERNEL_BIN_BASE_ADDR
mov ecx, KERNEL_SECTOR_NUMBER

call rd_disk_m32

call setup_page

sgdt [gdt_ptr]

mov ebx, [gdt_ptr + 2]
or dword [ebx+ 0x18 + 4], 0xc0000000

add dword [gdt_ptr + 2], 0xc0000000

add esp, 0xc0000000

; update page table addr
mov eax, PAGE_DIR_TABLE_POS
mov cr3, eax

; open cr0 PG bit (31th)
mov eax,cr0
or eax, 0x80000000
mov cr0, eax

; reload gdt after enable PageTable
lgdt [gdt_ptr]

; flush pipeline, not necessary
   jmp SELECTOR_CODE:enter_kernel
enter_kernel:
    call kernel_init
	mov esp, 0xc009f000

	jmp KERNEL_ENTRY_POINT
    ;jmp 0x1500

;=====================================================================================
;=====================================================================================

;--------- parse kernel.bin segment to its VA------------
kernel_init:
	xor eax, eax
	xor ebx, ebx	; program header address
	xor ecx, ecx	; number of program header
	xor edx, edx	; size of program header, e.g. e_phentsize
	mov dx, [KERNEL_BIN_BASE_ADDR + 42]	; e_phentsize
	mov ebx, [KERNEL_BIN_BASE_ADDR + 28]	;e_phoff, e.g. the first program header's offset in the file
	add ebx, KERNEL_BIN_BASE_ADDR
	mov cx, [KERNEL_BIN_BASE_ADDR + 44]		;e_phnum, e.g. number of program header
.each_segment_loop:
   cmp byte [ebx + 0], PT_NULL		  ; this program  header doesn't use if p_type== PT_NULL,
   je .PTNULL

   ;call memcpy(dst,src,size)
   push dword [ebx + 16]		  ; program header offset 16 bit :p_filesz, this is parameter: size
   mov eax, [ebx + 4]			  ; program header offset 4 bit :p_offset
   add eax, KERNEL_BIN_BASE_ADDR	  ; add kernel.bin addr , eax is physical addr
   push eax				  			; push src VA
   push dword [ebx + 8]			  ; push target VA : program header offset 8 bit p_vaddr
   call memcpy				  ; 
   add esp,12				  ; clear parameters in stack
.PTNULL:
   add ebx, edx				  ;add e_phentsize, e.g. edx(size of program header), make ebx point to next program header 
   loop .each_segment_loop
   ret


;----------------function-------------------------------
;--------------- memcpy(dst,src,size) ------------------
memcpy:
    cld
    push ebp
    mov ebp, esp
    push ecx    ;backup
    mov edi, [ebp+8]    ;dst 
    mov esi, [ebp+12]   ;src
    mov ecx, [ebp+16]   ;size
    rep movsb   ;copy

    pop ecx
    pop ebp
    ret



;-------------- create PDT &PTE -------------
setup_page:

;clear directory first
    mov ecx, 4096
    xor esi, esi
.clear_page_dir:
    mov byte [PAGE_DIR_TABLE_POS + esi], 0
    inc esi
    loop .clear_page_dir

;create (first & last) PDE
.create_pdt:
    mov eax,PAGE_DIR_TABLE_POS
    add eax,0x1000
    mov ebx,eax
    or eax, PG_US_U | PG_RW_W | PG_P
    mov [PAGE_DIR_TABLE_POS + 0x0], eax     ;first PDE
    mov [PAGE_DIR_TABLE_POS + 0xc00], eax   ;0xc00 means 768th PDE, indicating 0xc0000000~ mem 
    sub eax, 0x1000
    mov [PAGE_DIR_TABLE_POS +  4092], eax        ;last PDE point to PDE start addr

;create PTE
    mov ecx, 256
    xor esi,esi
    mov edx, PG_US_U | PG_RW_W | PG_P
.create_pte:
    mov [ebx+esi*4],edx     ;ebx:0x101000, write each PTE
    add edx,4096            ;each PTE control 4K mem
    inc esi
    loop .create_pte

;create kernel's other PDE
    mov eax, PAGE_DIR_TABLE_POS
    add eax,0x2000  ;second PDE addr
    or eax,PG_US_U | PG_RW_W | PG_P
    mov ebx, PAGE_DIR_TABLE_POS
    mov ecx,254     ;769~1022th PDE
    mov esi,769
.create_kernel_pde:
    mov [ebx+esi*4], eax
    inc esi
    add eax, 0x1000
    loop .create_kernel_pde
    ret 


;-------------------read disk in x86 mode---------------
; eax: LBA sector index
; ebx: target mem addr
; ecx: number of sector to read     ;only use cl
;-------------------------------------------------------
rd_disk_m32:

    mov esi,eax     ;backup
    mov di,cx

;set number of sectors to read
    mov dx,0x1f2
    mov al,cl
    out dx,al   

    mov eax,esi     ;cover eax

;set LBA index
    ;LBA 0~7 bit
    mov dx,0x1f3
    out dx,al
    ;LBA 8~15 bit
    mov cl,8
    shr eax,cl
    mov dx,0x1f4
    out dx,al
    ;LBA 16~32 bit
    shr eax,cl
    mov dx,0x1f5
    out dx,al

    shr eax,cl  ;LBA 24~27 bit 
    and al,0x0f
    or al,0xe0      ;7~4 : 1110b ,means LBA mode
    mov dx,0x1f6
    out dx,al

;set read command 0x20
    mov dx,0x1f7
    mov al,0x20
    out dx,al

.not_ready:
    nop
    in al,dx
    and al,0x88     ; 4th bit 1:disk ready  ; 7th bit 1: disk busy
    cmp al,0x08
    jnz .not_ready

    mov ax,di
    mov dx, 256
    mul dx
    mov cx,ax
    mov dx,0x1f0

.go_on_read:
    in ax,dx
    mov [ebx], ax
    add ebx,2
    loop .go_on_read
    ret 


String:
    db "loader started"
String_end:

