
# My-Own-OS

My-Own-OS is a bootloader and basic kernel made using ASM & C. Anyone can use this repository to understand how a OS works on
on a fundamental level and modify it to add functionality or tweak around to grasp how things work.

The Code is written is a easy to read for format for beginners to understand how a simple OS functions.

## Roadmap

- Make a simple Bootloader in ASM - DONE!

- Shift from 16 to 32 Bit - DONE!

- Jump from Assembly to C - DONE!

- Interrupt handling - DONE!
- Screen output and keyboard input - DONE!
- A tiny, basic libc which grows to suit our needs - DONE!
- Memory management - WIP
- Write a filesystem to store files
- Create a very simple shell
- User mode
- Maybe we will write a simple text editor
- Multiple processes and scheduling
- A BASIC interpreter, like in the 70s!
- A GUI
- Networking

## Installation

### Only compiling on Linux is supported

Required packages
-----------------

First, install the required packages. On linux, use your package distribution.

- gmp
- mpfr
- libmpc
- gcc 

Once installed, find where your packaged gcc is and export it. For example:

```
export CC=/usr/local/bin/gcc-4.9
export LD=/usr/local/bin/gcc-4.9
```

We will need to build binutils and a cross-compiled gcc, and we will put them into `/usr/local/i386elfgcc`, so
let's export some paths now. Feel free to change them to your liking.

```
export PREFIX="/usr/local/i386elfgcc"
export TARGET=i386-elf
export PATH="$PREFIX/bin:$PATH"
```

binutils
--------


```sh
mkdir /tmp/src
cd /tmp/src
curl -O http://ftp.gnu.org/gnu/binutils/binutils-2.24.tar.gz # If the link 404's, look for a more recent version
tar xf binutils-2.24.tar.gz
mkdir binutils-build
cd binutils-build
../binutils-2.24/configure --target=$TARGET --enable-interwork --enable-multilib --disable-nls --disable-werror --prefix=$PREFIX 2>&1 | tee configure.log
make all install 2>&1 | tee make.log
```

gcc
---
```sh
cd /tmp/src
curl -O https://ftp.gnu.org/gnu/gcc/gcc-4.9.1/gcc-4.9.1.tar.bz2
tar xf gcc-4.9.1.tar.bz2
mkdir gcc-build
cd gcc-build
../gcc-4.9.1/configure --target=$TARGET --prefix="$PREFIX" --disable-nls --disable-libssp --enable-languages=c --without-headers
make all-gcc 
make all-target-libgcc 
make install-gcc 
make install-target-libgcc 
```

```bash
  git clone https://github.com/SoumishDas/My-Own-OS.git
  cd My-Own-OS

  make run
```

## Feedback

If you have any feedback, please reach out to us at soumish.das@gmail.com


## License

[MIT License](https://choosealicense.com/licenses/mit/)


## Contributing

Contributions are always welcome!

We encourage contributions of any kind !

Please adhere to this project's `code of conduct` and make sure to not include maliccious code.




