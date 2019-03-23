
.PHONY=run 

run: write
	bochs -f bochsrc

write:
	$(MAKE) -C boot write
	$(MAKE) -C kernel write

clean:
	$(MAKE) -C boot clean
	$(MAKE) -C kernel clean
	rm -f bochs.out 
	dd if=/dev/zero of=hd.img bs=512 count=400 conv=notrunc
