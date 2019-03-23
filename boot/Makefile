
TARGET_IMG_PATH:= ../hd.img

.PHONY=write 


mbr.bin: mbr.S
	nasm -o mbr.bin mbr.S 
loader.bin: loader.S
	nasm -o loader.bin loader.S

#run:write
#	bochs -f bochsrc

write: mbr.bin loader.bin hd.img
	dd if=mbr.bin of=$(TARGET_IMG_PATH) bs=512 count=1 conv=notrunc
	dd if=loader.bin of=$(TARGET_IMG_PATH) bs=512 count=4  seek=2 conv=notrunc

clean:
	rm -f *.bin bochs.out 

hd.img:
	bximage -hd -mode="flat" -size=60 -q hd.img
	mv hd.img $(TARGET_IMG_PATH)
