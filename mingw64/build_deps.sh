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

    local uri="https://zlib.net/zlib-1.2.11.tar.gz"
    local archive_root='zlib-1.2.11'
    local sha256="c3e5e9fdd5004dcb542feda5ee4f0ff0744628baf8ed2dd5d66f8ca1197cb1a1"
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

    local uri="https://code.videolan.org/videolan/x264/-/archive/5db6aa6cab1b146e07b60cc1736a01f21da01154/x264-5db6aa6cab1b146e07b60cc1736a01f21da01154.zip"
    local archive_root='x264-5db6aa6cab1b146e07b60cc1736a01f21da01154'
    local sha256="6896825756ebe2c2657580d7636ed5fd6a41fea8e7722ba866fac7be6780d02f"
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

    local uri="https://archive.mozilla.org/pub/opus/opus-1.3.1.tar.gz"
    local archive_root='opus-1.3.1'
    local sha256="65b58e1e25b2a114157014736a3d9dfeaad8d41be1c8179866f144a2fb44ff9d"
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

    local uri="http://ffmpeg.org/releases/ffmpeg-4.4.tar.xz"
    local archive_root='ffmpeg-4.4'
    local sha256="06b10a183ce5371f915c6bb15b7b1fffbe046e8275099c96affc29e17645d909"
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

    local uri="https://www.libsdl.org/release/SDL2-2.0.14.tar.gz"
    local archive_root='SDL2-2.0.14'
    local sha256="d8215b571a581be1332d2106f8036fcb03d12a70bae01e20f424976d275432bc"
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

    local uri="https://github.com/anholt/libepoxy/releases/download/1.5.5/libepoxy-1.5.5.tar.xz"
    local archive_root='libepoxy-1.5.5'
    local sha256="261663db21bcc1cc232b07ea683252ee6992982276536924271535875f5b0556"
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