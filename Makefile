all: run

.PHONY=clean run-qemu run

run-qemu: bootsect
	@qemu-system-i386 -boot a -fda bootsect.o

run: write_disk 
	bochs 

mbr.bin:
	nasm -o mbr.bin mbr.S

write_disk: hd.img mbr.bin 
	dd if=mbr.bin of=hd.img bs=512 count=1 conv=notrunc

clean:
	@ - rm -f *.o mbr.bin 

clean_all:
	- dd if=/dev/zero of=hd.img bs=512 count=10 conv=notrunc

hd.img:
	bximage -hd -mode="flat" -size=60 -q hd.img
