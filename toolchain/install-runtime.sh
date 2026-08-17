#!/bin/sh
# Build and install the OS-specific portion layered over i686-elf Newlib.
set -eu

script_directory=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_root=$(CDPATH= cd -- "$script_directory/.." && pwd)
myos_prefix=${MYOS_TOOLCHAIN_PREFIX:-/home/soumish/opt/cross}
cross_prefix=${CROSS_COMPILE:-$myos_prefix/bin/i686-elf-}
target_root=$myos_prefix/i686-elf
runtime_root=$target_root/lib/myos
temporary_directory=$(mktemp -d)
trap 'rm -rf "$temporary_directory"' EXIT HUP INT TERM

if [ ! -f "$target_root/lib/libc.a" ] || [ ! -d "$target_root/include" ]; then
    echo "Newlib is not installed below $target_root." >&2
    echo "Follow user/newlib/README.md first." >&2
    exit 1
fi

mkdir -p "$runtime_root" "$myos_prefix/bin"

"${cross_prefix}gcc" -m32 -O2 -g -fno-pie -fno-pic \
    -fno-stack-protector -I"$target_root/include" \
    -I"$project_root/user/include" \
    -c "$project_root/user/newlib/syscalls.c" \
    -o "$temporary_directory/syscalls.o"

"${cross_prefix}ar" rcs "$runtime_root/libmyos.a" \
    "$temporary_directory/syscalls.o"
"${cross_prefix}ranlib" "$runtime_root/libmyos.a"

nasm -f elf32 -g -F dwarf "$project_root/user/newlib/crt0.s" \
    -o "$runtime_root/crt0.o"
nasm -f elf32 -g -F dwarf "$project_root/user/signal.s" \
    -o "$runtime_root/signal.o"

install -m 0644 "$project_root/user/link.ld" "$runtime_root/myos.ld"
install -m 0755 "$script_directory/bin/i686-myos-gcc" \
    "$myos_prefix/bin/i686-myos-gcc"

echo "Installed $myos_prefix/bin/i686-myos-gcc"
