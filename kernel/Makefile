TARGET_IMG_PATH:= ../hd.img
INCLUDE_KERNEL_PATH:= ../lib/kernel ../lib/ ../kernel/
GCC_FLAGS:= -c -fno-builtin -m32

REQUIRE_OUT_FILE:= kernel.o main.o init.o interrupt.o timer.o print.o
FILE_PATH

.PHONY=write clean

#kernel.bin:
#	gcc -c -o main.o main.c
#	ld main.o -Ttext 0xc0001500 -e main -o kernel.bin



clean:
	rm -f *.bin *.o

print.o: ../lib/kernel/print.S
	nasm -f elf32 -o ../lib/kernel/print.o ../lib/kernel/print.S
kernel.o: kernel.S
	nasm -f elf32 -o kernel.o kernel.S
	
main.o: main.c
	gcc  -I $(INCLUDE_KERNEL_PATH) $(GCC_FLAGS) -o main.o main.c 
	
init.o: init.c
	gcc  -I $(INCLUDE_KERNEL_PATH) $(GCC_FLAGS) -o init.o init.c 

interrupt.o: interrupt.c
	gcc  -I $(INCLUDE_KERNEL_PATH) $(GCC_FLAGS) -o interrupt.o interrupt.c 

kernel.bin: $(REQUIRE_OUT_FILE)
	ld -Ttext 0xc0001500 -e main -o kernel.bin  $(REQUIRE_OUT_FILE) ../lib/kernel/print.o ../device -m elf_i386
	
