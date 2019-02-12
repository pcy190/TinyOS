.code16


.text

.equ DEMOSEG,0X1020
.equ LEN, 20

show_text:
	mov $DEMOSEG, %ax
	mov %ax, %es
	mov $0x03,%ah
	xor %bh, %bh
	int $0x10

	mov $0x000a, %bx
	mov $0x1301, %ax
	mov $LEN, %cx
	mov $msg, %bp
	int $0x10	

loop_forever:
	jmp loop_forever

msg:
	.ascii "You've sucessfully load the floppy disk into RAM!"
	.byte 13,10

