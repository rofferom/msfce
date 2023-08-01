#!/bin/bash

set -e

BASE_DIR=$(realpath $(dirname $0))

# Build configuration
CROSS_PREFIX="x86_64-w64-mingw32"
NJOBS=$(grep -c ^processor /proc/cpuinfo)

# Intermediate directories
SRC_DIR="$BASE_DIR/src"
WORK_DIR="$BASE_DIR/work"

# Install folder + pkg-config configuration
ROOTFS_DIR="$BASE_DIR/$CROSS_PREFIX"
export PKG_CONFIG_LIBDIR=$ROOTFS_DIR/lib/pkgconfig

panic() {
    echo $1
    exit 1
}

test_sha256() {
    local local_path=$1
    local expected_hash=$2

    local real_hash=$(sha256sum $local_path | awk '{print $1}')
    
    if [[ $real_hash == $expected_hash ]]; then
        return 0
    else
        return 1
    fi
}

download_tarball() {
    local uri=$1
    local tarball_hash=$2

    local filename=$(basename $uri)
    local local_path="$SRC_DIR/$filename"

    echo "Downloading $local_path"

    # Check if file exists and is valid
    if [[ -f $local_path ]]; then
        test_sha256 $local_path $tarball_hash

        if [[ $? -eq 0 ]]; then
            echo "File already exists and is valid"
            return 0
        fi

        echo "File found, but invalid"
        rm $local_path
    fi

    # Get and check file integrity    
    wget $uri -O $local_path
    test_sha256 $local_path $tarball_hash
    if [[ $? -eq 0 ]]; then
        return 0
    fi

    return 1
}

build_zlib() {
    local init_cwd=$(pwd)

    local uri="https://zlib.net/zlib-1.2.13.tar.xz"
    local archive_root='zlib-1.2.13'
    local sha256="d14c38e313afc35a9a8760dadf26042f51ea0f5d154b0630a31da0540107fb98"
    local work_dir="$WORK_DIR/zlib"

    # Get sources
    download_tarball $uri $sha256 || panic "Fail to get zlib sources"
    mkdir -p $work_dir
    tar xf "$SRC_DIR/$(basename $uri)" -C $work_dir

    # Build
    cd "$work_dir/$archive_root"
    make -j$NJOBS -f win32/Makefile.gcc PREFIX=$CROSS_PREFIX-

    make -j$NJOBS -f win32/Makefile.gcc \
        prefix=$ROOTFS_DIR \
        INCLUDE_PATH=$ROOTFS_DIR/include \
        LIBRARY_PATH=$ROOTFS_DIR/lib \
        BINARY_PATH=$ROOTFS_DIR/bin \
        install

    cd $init_cwd
}

build_x264() {
    local init_cwd=$(pwd)

    local uri="https://code.videolan.org/videolan/x264/-/archive/baee400fa9ced6f5481a728138fed6e867b0ff7f/x264-baee400fa9ced6f5481a728138fed6e867b0ff7f.zip"
    local archive_root='x264-baee400fa9ced6f5481a728138fed6e867b0ff7f'
    local sha256="8e89a2074a0ba89267f7b4858714bdfe61cd1fea726a589788fc1d83d21579aa"
    local work_dir="$WORK_DIR/x264"

    # Get sources
    download_tarball $uri $sha256 || panic "Fail to get x264 sources"
    mkdir -p $work_dir
    unzip "$SRC_DIR/$(basename $uri)" -d $work_dir

    # Build
    cd "$work_dir/$archive_root"
    ./configure --host=$CROSS_PREFIX --cross-prefix=$CROSS_PREFIX- --disable-cli --enable-static --prefix=$ROOTFS_DIR
    make -j$NJOBS
    make install
    cd $init_cwd
}

build_opus() {
    local init_cwd=$(pwd)

    local uri="https://downloads.xiph.org/releases/opus/opus-1.4.tar.gz"
    local archive_root='opus-1.4'
    local sha256="c9b32b4253be5ae63d1ff16eea06b94b5f0f2951b7a02aceef58e3a3ce49c51f"
    local work_dir="$WORK_DIR/opus"

    # Get sources
    download_tarball $uri $sha256 || panic "Fail to get opus sources"
    mkdir -p $work_dir
    tar xf "$SRC_DIR/$(basename $uri)" -C $work_dir

    # Build
    cd "$work_dir/$archive_root"

    patch -p1 < $BASE_DIR/opus/0001-configure-Disable-FORTIFY_SOURCE.patch

    ./configure --host=$CROSS_PREFIX --prefix=$ROOTFS_DIR \
        --disable-extra-programs --disable-shared

    make -j$NJOBS
    make install

    cd $init_cwd
}

build_ffmpeg() {
    local init_cwd=$(pwd)

    local uri="https://ffmpeg.org/releases/ffmpeg-5.1.3.tar.xz"
    local archive_root='ffmpeg-5.1.3'
    local sha256="1b113593ff907293be7aed95acdda5e785dd73616d7d4ec90a0f6adbc5a0312e"
    local work_dir="$WORK_DIR/ffmpeg"

    # Get sources
    download_tarball $uri $sha256 || panic "Fail to get ffmpeg sources"
    mkdir -p $work_dir
    tar xf "$SRC_DIR/$(basename $uri)" -C $work_dir

    # Build
    cd "$work_dir/$archive_root"
    ./configure \
        --arch=x86_64 --target-os=mingw32 --cross-prefix=$CROSS_PREFIX- --pkg-config=pkg-config --prefix=$ROOTFS_DIR \
        --enable-gpl --enable-shared --disable-programs --disable-everything --disable-bsfs \
        --enable-swscale \
        --enable-libx264 --enable-encoder=libx264 \
        --enable-libopus --enable-encoder=libopus \
        --enable-encoder=png \
        --enable-muxer=matroska \
        --enable-muxer=mp4 \
        --enable-protocol=file

    make -j$NJOBS
    make install

    cd $init_cwd
}

build_sdl2() {
    local init_cwd=$(pwd)

    local uri="https://github.com/libsdl-org/SDL/archive/refs/tags/release-2.28.1.tar.gz"
    local archive_root='SDL-release-2.28.1'
    local sha256="99149dfaa45972fc29b1184517003dfb25ff44c9137b2b83d62eeae4cdc32a35"
    local work_dir="$WORK_DIR/sdl2"

    # Get sources
    download_tarball $uri $sha256 || panic "Fail to get SDL2 sources"
    mkdir -p $work_dir
    tar xf "$SRC_DIR/$(basename $uri)" -C $work_dir

    # Build
    cd "$work_dir/$archive_root"
    ./configure --host=$CROSS_PREFIX --prefix=$ROOTFS_DIR

    make -j$NJOBS
    make install

    cd $init_cwd
}

build_glm() {
    local init_cwd=$(pwd)

    local uri="https://github.com/g-truc/glm/releases/download/0.9.9.8/glm-0.9.9.8.zip"
    local archive_root='glm'
    local sha256="37e2a3d62ea3322e43593c34bae29f57e3e251ea89f4067506c94043769ade4c"
    local work_dir="$WORK_DIR/glm"

    # Get sources
    download_tarball $uri $sha256 || panic "Fail to get glm sources"
    mkdir -p $work_dir
    unzip "$SRC_DIR/$(basename $uri)" -d $work_dir

    # Install headers
    cd "$work_dir/$archive_root"
    mkdir -p $ROOTFS_DIR/include $ROOTFS_DIR/lib/pkgconfig
    cp -a glm "$ROOTFS_DIR/include"
    cp "$BASE_DIR/glm/glm.pc.in" "$ROOTFS_DIR/lib/pkgconfig/glm.pc"

    # Patch .pc file
    sed -i 's|@prefix@|'$ROOTFS_DIR'|g' "$ROOTFS_DIR/lib/pkgconfig/glm.pc"

    cd $init_cwd
}

build_epoxy() {
    local init_cwd=$(pwd)

    local uri="https://github.com/anholt/libepoxy/archive/refs/tags/1.5.10.tar.gz"
    local archive_root='libepoxy-1.5.10'
    local sha256="a7ced37f4102b745ac86d6a70a9da399cc139ff168ba6b8002b4d8d43c900c15"
    local work_dir="$WORK_DIR/epoxy"

    # Get sources
    download_tarball $uri $sha256 || panic "Fail to get epoxy sources"
    mkdir -p $work_dir
    tar xf "$SRC_DIR/$(basename $uri)" -C $work_dir

    # Build
    cd "$work_dir/$archive_root"
    mkdir build
    cd build
    meson .. --cross-file $BASE_DIR/meson-toolchain.ini --default-library=static --buildtype=release --prefix=$ROOTFS_DIR

    ninja
    ninja install

    cd $init_cwd
}

# Create folders
mkdir -p $SRC_DIR $WORK_DIR
mkdir -p $ROOTFS_DIR

# Build libs
build_zlib
build_x264
build_opus
build_ffmpeg
build_sdl2
build_glm
build_epoxy