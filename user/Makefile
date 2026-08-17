# Build static 32-bit userspace ELF files without using the host libc.
CROSS_COMPILE ?=
CC             := $(CROSS_COMPILE)gcc
LD             := $(CROSS_COMPILE)ld
NASM           ?= nasm

CFLAGS := -m32 -ffreestanding -nostdlib -nostdinc -fno-builtin \
	-fno-pie -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables \
	-fno-omit-frame-pointer -Wall -Wextra -O0 -g -std=gnu99 -Iinclude
LDFLAGS := -m elf_i386 -T link.ld --build-id=none
ASFLAGS := -f elf32 -g -F dwarf

RUNTIME_OBJECTS := build/crt0.o build/syscall.o

.PHONY: all clean
all: bin/hello

bin/hello: $(RUNTIME_OBJECTS) build/hello.o link.ld | bin
	$(LD) $(LDFLAGS) -Map bin/hello.map -o $@ $(RUNTIME_OBJECTS) build/hello.o

build/crt0.o: crt0.s | build
	$(NASM) $(ASFLAGS) $< -o $@

build/syscall.o: libc/syscall.c include/syscall.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/hello.o: programs/hello.c include/syscall.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build bin:
	mkdir -p $@

clean:
	rm -rf build bin
