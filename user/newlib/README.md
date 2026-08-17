# Newlib integration

`syscalls.c` is the OS adaptation layer for an otherwise unmodified Newlib.
`crt0.s` enters `main` and uses Newlib `exit` so buffered streams are flushed.
`demo.c` exercises formatted I/O, allocation, strings, and initrd file I/O.

## Install Newlib into the existing cross-toolchain

The commands below use upstream Newlib 4.6.0 and the existing compiler under
`/home/soumish/opt/cross`. Newlib requires a build directory separate from its
source directory.

```sh
export PATH=/home/soumish/opt/cross/bin:$PATH
mkdir -p /home/soumish/src /home/soumish/build
cd /home/soumish/src

curl -LO https://sourceware.org/pub/newlib/newlib-4.6.0.20260123.tar.gz
tar -xzf newlib-4.6.0.20260123.tar.gz

mkdir -p /home/soumish/build/newlib-i686-elf
cd /home/soumish/build/newlib-i686-elf

/home/soumish/src/newlib-4.6.0.20260123/configure \
  --target=i686-elf \
  --prefix=/home/soumish/opt/cross \
  --disable-newlib-supplied-syscalls \
  --disable-newlib-multithread \
  --enable-newlib-reent-small \
  --disable-nls

make -j4 all-target-newlib
make install-target-newlib
```

`--disable-newlib-supplied-syscalls` is essential: it leaves the OS boundary
for this repository's `syscalls.c` instead of linking a simulator/board stub.

## Build and run the demonstration

```sh
cd /mnt/c/users/soumi/downloads/user_mode/user

make newlib-demo \
  CROSS_COMPILE=/home/soumish/opt/cross/bin/i686-elf- \
  NEWLIB_PREFIX=/home/soumish/opt/cross

cd ../src
make run-newlib CROSS_COMPILE=/home/soumish/opt/cross/bin/i686-elf-
```

Inside the shell, execute `run /bin/newlib-demo`.
