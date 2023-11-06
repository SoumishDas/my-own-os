CFLAGS=-g -std=gnu99 -ffreestanding -O0 -Wall -Wextra
CC = /usr/local/i386elfgcc/bin/i386-elf-gcc
LDFLAGS=-T linker.ld -ffreestanding -O2 -nostdlib -lgcc
ASFLAGS=-f elf32
SRC_DIR := ./src
BUILD_DIR := ./build

# Locate all source files recursively in the source directory and its subdirectories
SOURCES := $(wildcard $(SRC_DIR)/**/*.c $(SRC_DIR)/**/*.s)
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))
OBJECTS := $(patsubst $(SRC_DIR)/%.s,$(BUILD_DIR)/%.o,$(OBJECTS))

#CRTI_OBJ=$(BUILD_DIR)/crti.o
CRTBEGIN_OBJ:=$(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJ:=$(shell $(CC) $(CFLAGS) -print-file-name=crtend.o)
#CRTN_OBJ=$(BUILD_DIR)/crtn.o

OBJ_LINK_LIST:= $(CRTBEGIN_OBJ) $(OBJECTS) $(CRTEND_OBJ) 
INTERNAL_OBJS:= $(OBJECTS) 

run : all
	qemu-system-x86_64 -kernel myos.bin -m 4G
# type=pc-i440fx-3.1
run_iso: iso
	qemu-system-x86_64 -cdrom myos.iso -m 4G

# Open the connection to qemu and load our kernel-object file with symbols
debug: all
	qemu-system-i386 -m 4G -s -S -kernel myos.bin -d guest_errors,int &
	gdb -ex "target remote localhost:1234" -ex "symbol-file myos.bin" -ex "set arch i386"

iso: all
	mkdir -p isodir/boot/grub
	cp myos.bin isodir/boot/myos.bin
	cp grub.cfg isodir/boot/grub/grub.cfg
	cp initrd.img isodir/boot/
	grub-mkrescue -o myos.iso isodir

all: $(OBJ_LINK_LIST) link
	./check_multiboot.sh

clean:
	-rm -rf $(INTERNAL_OBJS) myos.bin

link:
	$(CC) -o myos.bin $(LDFLAGS) $(OBJ_LINK_LIST)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.s | $(BUILD_DIR)
	nasm $(ASFLAGS) $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
