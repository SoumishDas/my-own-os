# Complete cross-toolchain setup

This document rebuilds the complete toolchain currently installed below
`/home/soumish/opt/cross`. The commands are intended for Ubuntu or Ubuntu
running under WSL.

The resulting toolchain has two compiler drivers:

- `i686-elf-gcc` is the bare-metal cross compiler used to build the kernel and
  the small freestanding user programs.
- `i686-myos-gcc` is this project's hosted userspace compiler. It automatically
  supplies the Newlib headers, C library, startup code, system-call glue, and
  linker script, so a normal program can be compiled with
  `i686-myos-gcc hello.c -o hello`.

The versions documented here match the tools installed on the development
machine:

| Component | Version or location |
| --- | --- |
| GNU Binutils | 2.46.0 |
| GCC | 15.3.0, C language only |
| Newlib | 4.6.0 (2026-01-23 snapshot) |
| MyOS runtime | This repository's `toolchain/` and `user/newlib/` sources |
| Installation prefix | `/home/soumish/opt/cross` |

## 1. Install the host-side build tools

Binutils, GCC, and Newlib are built from source, but their build systems need a
native compiler and several development libraries. NASM builds this kernel's
assembly files, `debugfs` updates the supplied ext2 floppy without mounting it,
and QEMU runs the OS.

```sh
sudo apt update
sudo apt install -y \
  build-essential \
  bison \
  flex \
  gawk \
  texinfo \
  libgmp-dev \
  libmpfr-dev \
  libmpc-dev \
  libisl-dev \
  zlib1g-dev \
  libzstd-dev \
  curl \
  xz-utils \
  ca-certificates \
  nasm \
  qemu-system-x86 \
  e2fsprogs
```

`grub-pc-bin` and `xorriso` are not needed for the normal build because
`floppy.img` already contains GRUB. They are useful only if the boot image is
recreated later:

```sh
sudo apt install -y grub-pc-bin xorriso
```

## 2. Define the paths used by the remaining commands

Run this block in every shell used to build the toolchain. These are ordinary
variables: changing `MYOS_PROJECT_DIR` is enough if the repository is moved.

```sh
export MYOS_TARGET=i686-elf
export MYOS_PREFIX=/home/soumish/opt/cross
export MYOS_SOURCE_DIR=/home/soumish/src
export MYOS_BUILD_DIR=/home/soumish/build
export MYOS_PROJECT_DIR=/mnt/c/users/soumi/downloads/user_mode
export PATH="$MYOS_PREFIX/bin:$PATH"

mkdir -p "$MYOS_PREFIX" "$MYOS_SOURCE_DIR" "$MYOS_BUILD_DIR"
```

Make the compiler available automatically in future shells:

```sh
grep -qxF 'export PATH=/home/soumish/opt/cross/bin:$PATH' \
  /home/soumish/.bashrc || \
  printf '\nexport PATH=/home/soumish/opt/cross/bin:$PATH\n' \
  >> /home/soumish/.bashrc

source /home/soumish/.bashrc
```

## 3. Download the exact upstream sources

Keeping source and build directories separate matters. It allows a failed
build directory to be discarded without touching the downloaded source.

```sh
cd "$MYOS_SOURCE_DIR"

curl -fLO https://ftp.gnu.org/gnu/binutils/binutils-2.46.0.tar.xz
curl -fLO https://ftp.gnu.org/gnu/gcc/gcc-15.3.0/gcc-15.3.0.tar.xz
curl -fLO https://sourceware.org/pub/newlib/newlib-4.6.0.20260123.tar.gz

tar -xf binutils-2.46.0.tar.xz
tar -xf gcc-15.3.0.tar.xz
tar -xf newlib-4.6.0.20260123.tar.gz
```

The `-f` option makes `curl` fail on an HTTP error instead of quietly saving an
HTML error page as a tarball.

## 4. Build and install cross Binutils

This supplies target-aware tools such as `i686-elf-as`, `i686-elf-ld`,
`i686-elf-ar`, `i686-elf-objdump`, and `i686-elf-readelf`. The host's normal
linker cannot safely replace these tools because it is configured for Linux,
not this kernel and its ELF layout.

```sh
mkdir -p "$MYOS_BUILD_DIR/binutils-2.46.0-i686-elf"
cd "$MYOS_BUILD_DIR/binutils-2.46.0-i686-elf"

"$MYOS_SOURCE_DIR/binutils-2.46.0/configure" \
  --target="$MYOS_TARGET" \
  --prefix="$MYOS_PREFIX" \
  --with-sysroot \
  --disable-nls \
  --disable-werror

make -j"$(nproc)"
make install
```

Refresh the shell's command lookup after installing the first cross tools:

```sh
export PATH="$MYOS_PREFIX/bin:$PATH"
hash -r
i686-elf-ld --version | head -n 1
```

## 5. Build and install the bare-metal GCC

This is deliberately a C-only, 32-bit, bare-metal compiler. It is built without
host headers, threads, shared libraries, or a host C++ runtime because none of
those facilities exist in this OS yet. `libgcc` is still built because both the
kernel and userspace may need GCC's low-level arithmetic helper routines.

```sh
mkdir -p "$MYOS_BUILD_DIR/gcc-15.3.0-i686-elf"
cd "$MYOS_BUILD_DIR/gcc-15.3.0-i686-elf"

"$MYOS_SOURCE_DIR/gcc-15.3.0/configure" \
  --target="$MYOS_TARGET" \
  --prefix="$MYOS_PREFIX" \
  --disable-nls \
  --enable-languages=c \
  --without-headers \
  --disable-multilib \
  --disable-shared \
  --disable-threads \
  --disable-libssp \
  --disable-libquadmath \
  --disable-libgomp \
  --disable-libatomic \
  --disable-libstdcxx \
  --disable-werror

make -j"$(nproc)" all-gcc all-target-libgcc
make install-gcc install-target-libgcc
```

Verify that this is the cross compiler, not Ubuntu's native compiler:

```sh
i686-elf-gcc --version | head -n 1
i686-elf-gcc -dumpmachine
```

The second command must print `i686-elf`.

## 6. Build and install Newlib

Newlib supplies standard headers and static libraries including `stdio.h`,
`stdlib.h`, `libc.a`, and `libm.a`. It does not know how this OS performs I/O,
allocation, process exit, or other kernel operations. The next section installs
the project-specific glue that connects those library calls to the system-call
ABI.

```sh
mkdir -p "$MYOS_BUILD_DIR/newlib-4.6.0-i686-elf"
cd "$MYOS_BUILD_DIR/newlib-4.6.0-i686-elf"

"$MYOS_SOURCE_DIR/newlib-4.6.0.20260123/configure" \
  --target="$MYOS_TARGET" \
  --prefix="$MYOS_PREFIX" \
  --disable-newlib-supplied-syscalls \
  --disable-newlib-multithread \
  --enable-newlib-reent-small \
  --disable-nls

make -j"$(nproc)" all-target-newlib
make install-target-newlib
```

`--disable-newlib-supplied-syscalls` is essential. Without it, Newlib may use
generic board/simulator stubs instead of the implementations in
`user/newlib/syscalls.c`.

Check the main installed artifacts:

```sh
test -f "$MYOS_PREFIX/i686-elf/include/stdio.h"
test -f "$MYOS_PREFIX/i686-elf/lib/libc.a"
test -f "$MYOS_PREFIX/i686-elf/lib/libm.a"
```

No output from `test` means the file exists.

## 7. Install the MyOS compiler driver and runtime

This is the only repository-specific installation step. The script:

1. compiles `user/newlib/syscalls.c` into `libmyos.a`;
2. assembles the userspace entry point and signal-return trampoline;
3. installs the userspace linker script; and
4. installs the `i686-myos-gcc` wrapper beside the cross compiler.

```sh
cd "$MYOS_PROJECT_DIR"
chmod +x toolchain/install-runtime.sh toolchain/bin/i686-myos-gcc

MYOS_TOOLCHAIN_PREFIX="$MYOS_PREFIX" \
CROSS_COMPILE="$MYOS_PREFIX/bin/i686-elf-" \
./toolchain/install-runtime.sh
```

Rerun this command whenever any of these files changes:

- `user/newlib/syscalls.c`
- `user/newlib/crt0.s`
- `user/signal.s`
- `user/link.ld`
- `toolchain/bin/i686-myos-gcc`

The installed runtime should now contain:

```sh
find "$MYOS_PREFIX/i686-elf/lib/myos" -maxdepth 1 -type f -print | sort
```

Expected files are `crt0.o`, `signal.o`, `libmyos.a`, and `myos.ld`.

## 8. Verify the complete toolchain

First verify that every executable can be found:

```sh
command -v i686-elf-as
command -v i686-elf-ld
command -v i686-elf-gcc
command -v i686-myos-gcc
command -v nasm
command -v debugfs
command -v qemu-system-i386
```

Then compile one of the repository's ordinary hosted programs. This exercises
GCC, Newlib, the startup objects, the syscall library, the linker script, and
`libgcc` together:

```sh
cd "$MYOS_PROJECT_DIR"
i686-myos-gcc -O2 -g user/apps/normal.c -o /tmp/myos-normal
i686-elf-readelf -h /tmp/myos-normal
i686-elf-readelf -l /tmp/myos-normal
```

The ELF header must report `Class: ELF32`, `Machine: Intel 80386`, and an
executable file type. This only checks the build and link stages; the program
must be placed in the initrd and booted to test its system calls.

## 9. Build and run the OS

The kernel Makefile expects the full cross-tool prefix, including the trailing
hyphen:

```sh
cd "$MYOS_PROJECT_DIR/src"
make clean
make image CROSS_COMPILE="$MYOS_PREFIX/bin/i686-elf-"
make run CROSS_COMPILE="$MYOS_PREFIX/bin/i686-elf-"
```

Every `user/apps/*.c` source is automatically compiled with `i686-myos-gcc`
and added to the initrd as `/bin/<source-name>`. Therefore a new normal C
program can be added and run with:

```sh
cd "$MYOS_PROJECT_DIR"
cp /path/to/hello.c user/apps/hello-newlib.c

cd src
make run CROSS_COMPILE="$MYOS_PREFIX/bin/i686-elf-"
```

At the OS shell, run:

```text
run /bin/hello-newlib
```

## 10. Use the compiler directly

Once the setup is complete, a normal single-file program needs no
`-ffreestanding`, manual startup object, linker script, or explicit `-lc`:

```sh
i686-myos-gcc hello.c -o hello
```

Compilation-only commands work as expected:

```sh
i686-myos-gcc -Wall -Wextra -O2 -c hello.c -o hello.o
i686-myos-gcc -E hello.c -o hello.i
i686-myos-gcc -S hello.c -o hello.s
```

This compiler still targets the current MyOS ABI, not Linux. A program can use
only the Newlib features supported by the system calls in
`user/newlib/syscalls.c`; an API being declared in a standard header does not
automatically mean that the kernel implements its underlying operation.

## Troubleshooting

### `i686-elf-gcc: command not found`

Restore the cross-toolchain path in the current shell:

```sh
export PATH=/home/soumish/opt/cross/bin:$PATH
hash -r
```

### Newlib headers or `libc.a` are missing

Confirm that the prefix used for Newlib is the same one used for GCC:

```sh
ls -l /home/soumish/opt/cross/i686-elf/include/stdio.h
ls -l /home/soumish/opt/cross/i686-elf/lib/libc.a
```

If either path is missing, repeat section 6 from an empty Newlib build
directory.

### `i686-myos-gcc` reports a missing runtime file

Newlib is present but the OS-specific layer has not been installed, or it is
stale. Repeat section 7:

```sh
cd /mnt/c/users/soumi/downloads/user_mode
./toolchain/install-runtime.sh
```

### GCC or Binutils configuration appears to reuse old settings

These projects cache configuration in their build directories. Use a new,
empty directory name and run `configure` there; never run either configure
script inside its source tree.

### QEMU is installed but its window is unusable under WSL

That is a host display/WSL integration problem rather than a cross-compiler
problem. The kernel image can still be built with `make image`; QEMU display,
VNC, or serial options can be supplied separately through the `QEMU` Makefile
override.
