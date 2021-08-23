#!/bin/bash

set -e

BASE_DIR=$(realpath $(dirname $0))
NJOBS=$(grep -c ^processor /proc/cpuinfo)

# Toolchain components version
BINUTILS_VERSION=2.31
GCC_VERSION=8.3.0

# Toolchain configuration
TOOLCHAIN_TARGET=arm-linux-gnueabihf
TOOLCHAIN_ARCH=armv6
TOOLCHAIN_FPU=vfp
TOOLCHAIN_FLOAT=hard

# Download and build base directory
export BUILD_DIR=/root

# Installation directories
export TOOLCHAIN_BASEPATH=/opt/raspi1-toolchain
export TOOLCHAIN_HOST_SYSROOT=$TOOLCHAIN_BASEPATH/host_sysroot
export TOOLCHAIN_TARGET_SYSROOT=$TOOLCHAIN_BASEPATH/target_sysroot

# Setup env
export PATH=$TOOLCHAIN_HOST_SYSROOT/bin:$PATH
mkdir -p $TOOLCHAIN_HOST_SYSROOT $TOOLCHAIN_TARGET_SYSROOT

# Install target sysroot
wget https://github.com/ev3dev/raspbian-archive-keyring/raw/ev3dev-trusty/keyrings/raspbian-archive-keyring.gpg -P $TOOLCHAIN_TARGET_SYSROOT/etc/apt/trusted.gpg.d/
multistrap -f $BASE_DIR/raspi.conf -d $TOOLCHAIN_TARGET_SYSROOT
$BASE_DIR/fix_rootfs.sh $TOOLCHAIN_TARGET_SYSROOT

# Build host sysroot
# Build binutils
cd $BUILD_DIR
wget https://ftpmirror.gnu.org/binutils/binutils-$BINUTILS_VERSION.tar.bz2
tar xf binutils-$BINUTILS_VERSION.tar.bz2

mkdir -p $BUILD_DIR/build-binutils && cd $BUILD_DIR/build-binutils

$BUILD_DIR/binutils-$BINUTILS_VERSION/configure --prefix=$TOOLCHAIN_HOST_SYSROOT \
    --target=$TOOLCHAIN_TARGET --with-arch=$TOOLCHAIN_ARCH --with-fpu=$TOOLCHAIN_FPU --with-float=$TOOLCHAIN_FLOAT \
    --with-sysroot=$TOOLCHAIN_TARGET_SYSROOT --disable-multilib --disable-nls

make -j$NJOBS
make install

# Build GCC
cd $BUILD_DIR
wget https://ftpmirror.gnu.org/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.gz
tar xf gcc-$GCC_VERSION.tar.gz
cd $BUILD_DIR/gcc-$GCC_VERSION
./contrib/download_prerequisites

mkdir -p $BUILD_DIR/build-gcc && cd $BUILD_DIR/build-gcc

$BUILD_DIR/gcc-$GCC_VERSION/configure --prefix=$TOOLCHAIN_HOST_SYSROOT \
    --target=$TOOLCHAIN_TARGET --with-arch=$TOOLCHAIN_ARCH --with-fpu=$TOOLCHAIN_FPU --with-float=$TOOLCHAIN_FLOAT \
    --with-sysroot=$TOOLCHAIN_TARGET_SYSROOT --enable-languages=c,c++ --disable-multilib --disable-nls

make -j$NJOBS
make install
