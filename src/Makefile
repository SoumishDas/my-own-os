# Build the 32-bit freestanding kernel from Linux/WSL.
# Pipeline: sources -> ELF32 objects -> linked kernel -> existing ext2 floppy.
# GRUB is already installed in that floppy and loads /kernel plus /initrd.

# Example: CROSS_COMPILE=/home/user/opt/cross/bin/i686-elf-
CROSS_COMPILE ?=
CC             := $(CROSS_COMPILE)gcc
LD             := $(CROSS_COMPILE)ld
NASM           ?= nasm
DEBUGFS        ?= debugfs
QEMU           ?= qemu-system-i386
HOSTCC         ?= cc

TARGET         := kernel
LINKER_SCRIPT  := link.ld
MAP_FILE       := $(TARGET).map
FLOPPY_IMAGE   ?= ../floppy.img
INITRD_IMAGE   ?= ../initrd.img
INITRD_BUILDER := ../make_initrd
USER_DIRECTORY := ../user
HOSTED_USER_NAMES := $(notdir $(basename $(wildcard $(USER_DIRECTORY)/apps/*.c)))
HOSTED_INITRD_ARGUMENTS := $(foreach name,$(HOSTED_USER_NAMES),user/bin/$(name) bin/$(name))

C_SOURCES      := $(sort $(wildcard *.c))
ASM_SOURCES    := $(sort $(wildcard *.s))
# Keep the Multiboot header at the front of the image and discover new sources
# automatically after that.
OBJECTS        := boot.o $(filter-out boot.o,$(C_SOURCES:.c=.o) $(ASM_SOURCES:.s=.o))
DEP_FILES      := $(C_SOURCES:.c=.d)

# Remove hosted-runtime assumptions/features this kernel cannot initialize.
ARCH_CFLAGS    := -m32 -ffreestanding -nostdlib -nostdinc -fno-builtin \
	-fno-pie -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables \
	-fno-omit-frame-pointer
WARN_CFLAGS    := -Wall -Wextra
CFLAGS         ?= -O0 -g -std=gnu99
CPPFLAGS       ?=
DEPFLAGS       := -MMD -MP

ARCH_LDFLAGS   := -m elf_i386 -T $(LINKER_SCRIPT) --build-id=none
LDFLAGS        ?=
ARCH_ASFLAGS   := -f elf32
ASFLAGS        ?= -g -F dwarf

.DEFAULT_GOAL := all

.PHONY: all image run clean userspace help newlib-image run-newlib

all: $(TARGET)

userspace:
	$(MAKE) -C $(USER_DIRECTORY) CROSS_COMPILE=$(CROSS_COMPILE)

$(INITRD_BUILDER): ../make_initrd.c
	$(HOSTCC) -std=c99 -Wall -Wextra -O2 $< -o $@

$(INITRD_IMAGE): userspace $(INITRD_BUILDER) ../test.txt ../test2.txt
	cd .. && ./make_initrd test.txt test.txt \
		test2.txt docs/guides/test2.txt \
		user/bin/hello bin/hello \
		user/bin/args bin/args \
		user/bin/heapdemo bin/heapdemo \
		user/bin/status bin/status \
		user/bin/sigdemo bin/sigdemo \
		user/bin/abicheck bin/abicheck \
		user/bin/readcheck bin/readcheck \
		user/bin/shell bin/shell \
		$(HOSTED_INITRD_ARGUMENTS)

$(TARGET): $(OBJECTS) $(LINKER_SCRIPT)
	$(LD) $(ARCH_LDFLAGS) $(LDFLAGS) -Map $(MAP_FILE) -o $@ $(OBJECTS)

# The supplied floppy is an ext2 image. debugfs updates it without requiring a
# privileged loop mount, which makes this target convenient from WSL.
image: $(TARGET) $(FLOPPY_IMAGE) $(INITRD_IMAGE)
	-$(DEBUGFS) -w -R "rm /kernel" $(FLOPPY_IMAGE)
	$(DEBUGFS) -w -R "write $(TARGET) /kernel" $(FLOPPY_IMAGE)
	-$(DEBUGFS) -w -R "rm /initrd" $(FLOPPY_IMAGE)
	$(DEBUGFS) -w -R "write $(INITRD_IMAGE) /initrd" $(FLOPPY_IMAGE)

run: image
	$(QEMU) -drive file=$(FLOPPY_IMAGE),format=raw,if=floppy -boot a

# Package an already built optional Newlib demonstration without making every
# normal kernel build depend on an external Newlib installation.
newlib-image: image
	@test -f $(USER_DIRECTORY)/bin/newlib-demo || \
		(echo "Build user/newlib-demo first"; exit 1)
	cd .. && ./make_initrd test.txt test.txt \
		test2.txt docs/guides/test2.txt \
		user/bin/hello bin/hello user/bin/args bin/args \
		user/bin/heapdemo bin/heapdemo user/bin/status bin/status \
		user/bin/sigdemo bin/sigdemo user/bin/abicheck bin/abicheck \
		user/bin/readcheck bin/readcheck user/bin/shell bin/shell \
		user/bin/newlib-demo bin/newlib-demo \
		$(HOSTED_INITRD_ARGUMENTS)
	-$(DEBUGFS) -w -R "rm /initrd" $(FLOPPY_IMAGE)
	$(DEBUGFS) -w -R "write $(INITRD_IMAGE) /initrd" $(FLOPPY_IMAGE)

run-newlib: newlib-image
	$(QEMU) -drive file=$(FLOPPY_IMAGE),format=raw,if=floppy -boot a

%.o: %.c
	$(CC) $(CPPFLAGS) $(ARCH_CFLAGS) $(WARN_CFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

%.o: %.s
	$(NASM) $(ARCH_ASFLAGS) $(ASFLAGS) $< -o $@

clean:
	$(RM) $(OBJECTS) $(DEP_FILES) $(TARGET) $(MAP_FILE) shell.o shell.d
	$(MAKE) -C $(USER_DIRECTORY) clean

help:
	@echo "Targets:"
	@echo "  all    Build $(TARGET) (default)"
	@echo "  image  Build and copy the kernel/initrd into $(FLOPPY_IMAGE)"
	@echo "  userspace  Build standalone ELF32 user programs"
	@echo "  run    Update the floppy image and boot it with QEMU"
	@echo "  newlib-image  Package an already-built user/bin/newlib-demo"
	@echo "  run-newlib    Package Newlib demo and boot it with QEMU"
	@echo "  clean  Remove generated objects, dependencies, kernel, and map file"
	@echo ""
	@echo "Optional overrides: CROSS_COMPILE, CC, LD, NASM, DEBUGFS, QEMU,"
	@echo "                    CFLAGS, LDFLAGS, FLOPPY_IMAGE, INITRD_IMAGE"

-include $(DEP_FILES)
