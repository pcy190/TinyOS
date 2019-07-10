BUILD_PATH:= build
ENTRY_POINT = 0xc0001500


KERNEL_PATH:=./kernel
DEVICE_PATH:=./device
LIB_KERNEL_PATH= ./lib/kernel
LIB = -I lib/ -I lib/kernel/ -I lib/user/ -I kernel/ -I device/ -I thread/ -I userprog/ -I fs/
CFLAGS:= -c -m32  $(LIB) -fno-stack-protector -fno-builtin -Wall -W -Wstrict-prototypes -Wmissing-prototypes 
# -W -Wmissing-prototypes -Wsystem-headers
LDFLAGS = -Ttext $(ENTRY_POINT) -e main -m elf_i386
#-Map $(BUILD_DIR)/kernel.map

TARGET_IMG_PATH:= hd.img

OBJS = $(BUILD_PATH)/main.o $(BUILD_PATH)/init.o $(BUILD_PATH)/interrupt.o \
       $(BUILD_PATH)/timer.o $(BUILD_PATH)/kernel.o $(BUILD_PATH)/print.o  $(BUILD_PATH)/memory.o \
	   $(BUILD_PATH)/debug.o $(BUILD_DIR)/bitmap.o $(BUILD_DIR)/string.o $(BUILD_PATH)/thread.o \
	   $(BUILD_PATH)/list.o $(BUILD_PATH)/switch.o $(BUILD_PATH)/sync.o $(BUILD_PATH)/console.o \
	   $(BUILD_PATH)/ioqueue.o $(BUILD_PATH)/keyboard.o $(BUILD_PATH)/tss.o $(BUILD_PATH)/process.o \
	   $(BUILD_PATH)/syscall-init.o $(BUILD_PATH)/syscall.o $(BUILD_PATH)/stdio.o $(BUILD_PATH)/stdio-kernel.o $(BUILD_PATH)/ide.o \
	   $(BUILD_PATH)/fs.o $(BUILD_PATH)/file.o $(BUILD_PATH)/dir.o $(BUILD_PATH)/inode.o $(BUILD_PATH)/fork.o
	   
	 
AS = nasm
CC = gcc
LD = ld

.PHONY : mkdir run write clean build mk_dir restart clear_disk fdisk

    
run: write
	bochs -f bochsrc

clear_disk:
	dd if=/dev/zero of=hd80M.img bs=10M count=8 conv=notrunc

fdisk:
	fdisk hd80M.img < fdisk.txt

reformat_disk:
	make clear_disk
	make fdisk

clean:
	$(MAKE) -C boot clean
	#$(MAKE) -C kernel clean
	rm -rf build/*
	rm -f bochs.out 
	dd if=/dev/zero of=hd.img bs=4M count=10 conv=notrunc
	
restart:
	$(MAKE) -C ./ clean
	$(MAKE) -C ./ run

write: build
	$(MAKE) -C boot write
	dd if=$(BUILD_PATH)/kernel.bin of=$(TARGET_IMG_PATH) bs=512 count=300  seek=9 conv=notrunc
	
build: mk_dir $(BUILD_PATH)/kernel.bin

mk_dir:
	-mkdir build
	#if [ ! -d $(BUILD_DIR) ];then mkdir $(BUILD_DIR);fi
	
		
### compile
$(BUILD_PATH)/print.o: $(LIB_KERNEL_PATH)/print.S
	$(AS) -f elf32 $< -o $@
$(BUILD_PATH)/kernel.o: $(KERNEL_PATH)/kernel.S
	$(AS) -f elf32 $< -o $@
$(BUILD_PATH)/switch.o: thread/switch.S
	$(AS) -f elf32 $< -o $@

	
$(BUILD_PATH)/main.o: $(KERNEL_PATH)/main.c $(LIB_KERNEL_PATH)/print.h  lib/stdint.h $(KERNEL_PATH)/init.h
	$(CC) $(CFLAGS) $< -o $@
	
$(BUILD_PATH)/init.o: $(KERNEL_PATH)/init.c $(KERNEL_PATH)/init.h $(LIB_KERNEL_PATH)/print.h  lib/stdint.h $(KERNEL_PATH)/interrupt.h  $(DEVICE_PATH)/timer.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_PATH)/interrupt.o: $(KERNEL_PATH)/interrupt.c $(KERNEL_PATH)/interrupt.h lib/stdint.h $(KERNEL_PATH)/global.h $(LIB_KERNEL_PATH)/io.h $(LIB_KERNEL_PATH)/print.h
	$(CC) $(CFLAGS) $< -o $@
	
$(BUILD_PATH)/timer.o: $(DEVICE_PATH)/timer.c $(DEVICE_PATH)/timer.h lib/stdint.h $(LIB_KERNEL_PATH)/io.h $(LIB_KERNEL_PATH)/print.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_PATH)/memory.o: $(KERNEL_PATH)/memory.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/string.o: lib/string.c lib/string.h lib/stdint.h $(KERNEL_PATH)/global.h lib/stdint.h $(KERNEL_PATH)/debug.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/bitmap.o: $(LIB_KERNEL_PATH)/bitmap.c 
	$(CC) $(CFLAGS) $< -o $@
	
$(BUILD_PATH)/debug.o: $(KERNEL_PATH)/debug.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_PATH)/sync.o: thread/sync.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_PATH)/thread.o: thread/thread.c thread/switch.S
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_PATH)/list.o: $(LIB_KERNEL_PATH)/list.c 
	$(CC) $(CFLAGS) $< -o $@
	
$(BUILD_PATH)/console.o: $(DEVICE_PATH)/console.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_PATH)/ioqueue.o: $(DEVICE_PATH)/ioqueue.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_PATH)/keyboard.o: $(DEVICE_PATH)/keyboard.c
	$(CC) $(CFLAGS) $< -o $@
	
$(BUILD_PATH)/tss.o: userprog/tss.c
	$(CC) $(CFLAGS) $< -o $@
	
$(BUILD_PATH)/process.o: userprog/process.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_PATH)/fork.o: userprog/fork.c
	$(CC) $(CFLAGS) $< -o $@
	
$(BUILD_PATH)/syscall-init.o: userprog/syscall-init.c
	$(CC) $(CFLAGS) $< -o $@
	
$(BUILD_PATH)/syscall.o: lib/user/syscall.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_PATH)/stdio.o: lib/stdio.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_PATH)/stdio-kernel.o: lib/kernel/stdio-kernel.c
	$(CC) $(CFLAGS) $< -o $@
	
$(BUILD_PATH)/ide.o: device/ide.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_PATH)/inode.o: fs/inode.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_PATH)/file.o: fs/file.c
	$(CC) $(CFLAGS) $< -o $@
	
$(BUILD_PATH)/dir.o: fs/dir.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_PATH)/fs.o: fs/fs.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_PATH)/kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) $^ -o $@
	




	
	
	
