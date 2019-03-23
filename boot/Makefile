.PHONY=write run


mbr.bin: mbr.S
	nasm -o mbr.bin mbr.S 
loader.bin: loader.S
	nasm -o loader.bin loader.S

run:write
	bochs -f bochsrc

write: mbr.bin loader.bin
	dd if=mbr.bin of=hd.img bs=512 count=1 conv=notrunc
	dd if=loader.bin of=hd.img bs=512 count=4  seek=2 conv=notrunc

clean:
	rm -f *.bin bochs.out 
