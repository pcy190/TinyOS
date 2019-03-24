INCLUDE_PATH:= ../lib/kernel
BUILD_PATH:= ../build
TARGET_IMG_PATH:= ../hd.img

.PHONY=write clean

timer.o: timer.c
	gcc -I $(INCLUDE_PATH) -c -o timer.o timer.c

compile: timer.o

clean:
	rm -f timer.o
