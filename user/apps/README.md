# Ordinary C applications

Put one standalone program per `.c` file in this directory. The filename is
the executable name inside the OS. For example, `calculator.c` becomes
`user/bin/calculator` on the host and `/bin/calculator` in the initrd.

No Makefile edit is required:

```sh
cd /mnt/c/users/soumi/downloads/user_mode/src
make run CROSS_COMPILE=/home/soumish/opt/cross/bin/i686-elf-
```

Then launch it from the OS shell:

```text
run /bin/calculator argument1 argument2
```

For a source file outside this directory, compile directly with:

```sh
i686-myos-gcc program.c -o program
```

The kernel still uses `i686-elf-gcc` and freestanding flags. Only userspace
applications use the hosted `i686-myos-gcc` driver.
