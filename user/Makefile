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
NEWLIB_PREFIX ?=
MYOS_CC       ?= ../toolchain/bin/i686-myos-gcc

PROGRAMS        := hello args heapdemo status sigdemo abicheck readcheck shell
BINARIES        := $(addprefix bin/,$(PROGRAMS))
RUNTIME_OBJECTS := build/crt0.o build/signal.o build/syscall.o build/print.o
HOSTED_SOURCES  := $(sort $(wildcard apps/*.c))
HOSTED_PROGRAMS := $(notdir $(HOSTED_SOURCES:.c=))
HOSTED_BINARIES := $(addprefix bin/,$(HOSTED_PROGRAMS))

.PHONY: all clean newlib-demo
.SECONDARY:
all: $(BINARIES) $(HOSTED_BINARIES)

$(HOSTED_BINARIES): bin/%: apps/%.c | bin
	$(MYOS_CC) -O2 -g $< -o $@

bin/%: $(RUNTIME_OBJECTS) build/%.o link.ld | bin
	$(LD) $(LDFLAGS) -Map $@.map -o $@ $(RUNTIME_OBJECTS) build/$*.o

build/crt0.o: crt0.s | build
	$(NASM) $(ASFLAGS) $< -o $@

build/signal.o: signal.s | build
	$(NASM) $(ASFLAGS) $< -o $@

# This target is optional because ordinary kernel builds must not require an
# installed Newlib. NEWLIB_PREFIX contains i686-elf/include and i686-elf/lib.
newlib-demo: | build bin
	@test -n "$(NEWLIB_PREFIX)" || \
		(echo "Set NEWLIB_PREFIX to the Newlib installation prefix"; exit 1)
	$(NASM) $(ASFLAGS) newlib/crt0.s -o build/newlib-crt0.o
	$(CC) -m32 -ffreestanding -fno-pie -fno-pic -Wall -Wextra -O0 -g \
		-std=gnu11 -I$(NEWLIB_PREFIX)/i686-elf/include -Iinclude \
		-c newlib/syscalls.c -o build/newlib-syscalls.o
	$(CC) -m32 -ffreestanding -fno-pie -fno-pic -Wall -Wextra -O0 -g \
		-std=gnu11 -I$(NEWLIB_PREFIX)/i686-elf/include \
		-c newlib/demo.c -o build/newlib-demo.o
	$(CC) -m32 -nostdlib -no-pie \
		-Wl,-m,elf_i386,-T,link.ld,--build-id=none,-Map,bin/newlib-demo.map \
		-o bin/newlib-demo build/newlib-crt0.o build/signal.o build/newlib-syscalls.o \
		build/newlib-demo.o -L$(NEWLIB_PREFIX)/i686-elf/lib -lc -lm -lc -lgcc

build/syscall.o: libc/syscall.c include/syscall.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/print.o: libc/print.c include/userlib.h include/syscall.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: programs/%.c include/userlib.h include/syscall.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build bin:
	mkdir -p $@

clean:
	rm -rf build bin
