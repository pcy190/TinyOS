TARGET_IMG_PATH:= ../hd.img

.PHONY=write clean

#kernel.bin:
#	gcc -c -o main.o main.c
#	ld main.o -Ttext 0xc0001500 -e main -o kernel.bin

write: kernel.bin
	dd if=kernel.bin of=$(TARGET_IMG_PATH) bs=512 count=200  seek=9 conv=notrunc

clean:
	rm -f *.bin *.o

print.o: ../lib/kernel/print.S
	nasm -f elf32 -o ../lib/kernel/print.o ../lib/kernel/print.S
	
main.o: main.c
	gcc -m32 -I ../lib/kernel/ -c -o main.o main.c 

kernel.bin: print.o main.o
	ld -Ttext 0xc0001500 -e main -o kernel.bin main.o ../lib/kernel/print.o -m elf_i386
