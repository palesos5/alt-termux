#!/bin/sh
# toolchain.sh — тулчейн «как в 2004»: binutils 2.18 + gcc 4.2.4, target mips-elf.
# Никаких docker/conda/pip (технологии не той эпохи): только ./configure && make.
set -e
V_BIN=2.18   V_GCC=4.2.4
PREFIX=${PREFIX:-/opt/miyabi}
mkdir -p build-tc && cd build-tc

[ -f binutils-$V_BIN.tar.bz2 ] || wget https://ftp.gnu.org/gnu/binutils/binutils-$V_BIN.tar.bz2
tar xf binutils-$V_BIN.tar.bz2
cd binutils-$V_BIN
./configure --target=mips-elf --prefix=$PREFIX --disable-werror
make -j$(nproc) all && make install
cd ..

[ -f gcc-$V_GCC.tar.bz2 ] || wget https://ftp.gnu.org/gnu/gcc/gcc-$V_GCC/gcc-$V_GCC.tar.bz2
tar xf gcc-$V_GCC.tar.bz2
cd gcc-$V_GCC
./configure --target=mips-elf --prefix=$PREFIX --without-headers --with-newlib \
            --enable-languages=c --disable-libssp --disable-threads
make -j$(nproc) all-gcc && make install-gcc
echo "toolchain ready: $PREFIX/bin/mips-elf-gcc (gcc $V_GCC / binutils $V_BIN)"
