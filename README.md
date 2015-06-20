# Kpatch for JOS
## Peking University OS Lab Class class project

### Introduction

We implemented a kernel patch feature for JOS.

### Testing

#### Prerequisite

* ruby
* gcc-multilib
* qemu
* (and other prerequisite for JOS)

This test can only run from a Linux host OS currently (because of a bug from GCC)

#### Run

On host system (your own OS):
```sh
make qemu-nox
```

On guest system (JOS in qemu):
```sh

# test original kpatch_test1 and kpatch_test2
kcall kpatch_test1
kcall kpatch_test2

## test patch from kernel space ##

# patch kpatch_test1 using kpatch_test2
kpatch kpatch_test1 0xf0106f6d
# now kpatch_test1 is patched so that it would output "kpatch_test2"
kcall kpatch_test1

## test patch from user space ##
# insert kernel module test.ko get test2 address 0xf025200c
insmod test.ko
# patch kpatch_test1 using test2
kpatch kpatch_test1 0xf025200c
# now kpatch_test1 is patched so that it would output "Hello World"
kcall kpatch_test1
```

#### Reference environment

This is tested on:
Linux:
```
Linux ubuntu 3.13.0-32-generic #57-Ubuntu SMP Tue Jul 15 03:51:08 UTC 2014 x86_64 x86_64 x86_64 GNU/Linux
```

GCC:
```
Using built-in specs.
COLLECT_GCC=gcc
COLLECT_LTO_WRAPPER=/usr/lib/gcc/x86_64-linux-gnu/4.8/lto-wrapper
Target: x86_64-linux-gnu
Configured with: ../src/configure -v --with-pkgversion='Ubuntu 4.8.4-2ubuntu1~14.04' --with-bugurl=file:///usr/share/doc/gcc-4.8/README.Bugs --enable-languages=c,c++,java,go,d,fortran,objc,obj-c++ --prefix=/usr --program-suffix=-4.8 --enable-shared --enable-linker-build-id --libexecdir=/usr/lib --without-included-gettext --enable-threads=posix --with-gxx-include-dir=/usr/include/c++/4.8 --libdir=/usr/lib --enable-nls --with-sysroot=/ --enable-clocale=gnu --enable-libstdcxx-debug --enable-libstdcxx-time=yes --enable-gnu-unique-object --disable-libmudflap --enable-plugin --with-system-zlib --disable-browser-plugin --enable-java-awt=gtk --enable-gtk-cairo --with-java-home=/usr/lib/jvm/java-1.5.0-gcj-4.8-amd64/jre --enable-java-home --with-jvm-root-dir=/usr/lib/jvm/java-1.5.0-gcj-4.8-amd64 --with-jvm-jar-dir=/usr/lib/jvm-exports/java-1.5.0-gcj-4.8-amd64 --with-arch-directory=amd64 --with-ecj-jar=/usr/share/java/eclipse-ecj.jar --enable-objc-gc --enable-multiarch --disable-werror --with-arch-32=i686 --with-abi=m64 --with-multilib-list=m32,m64,mx32 --with-tune=generic --enable-checking=release --build=x86_64-linux-gnu --host=x86_64-linux-gnu --target=x86_64-linux-gnu
Thread model: posix
gcc version 4.8.4 (Ubuntu 4.8.4-2ubuntu1~14.04)
```

### Contributions

1. Patching a kernel-function with another kernel-function. See kern/kpatch.{h,c}.
2. Loadable kernel module (that workds only one-time). See modules/ and user/insmod.c.
3. User-space utilities. See user/kcall.c, user/kpatch.c and user/insmod.c

### Division of work

* 杨俊睿 Host system patch generatation
* 刘当一 Guest system patch toolchain

### Open Source!
Fork and Star us on [GitHub](https://github.com/jos-kpatch/jos-kpatch "jos-kpatch")!
