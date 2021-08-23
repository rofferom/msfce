#!/bin/bash

libs="libmount.so
      libdbus-1.so
      libsepol.so
      libnss_files.so
      librt.so
      libpthread.so
      libuuid.so
      libnss_hesiod.so
      libselinux.so
      libutil.so
      libz.so
      libanl.so
      libm.so
      libnss_nisplus.so
      libcrypt.so
      libnss_dns.so
      libnsl.so
      libthread_db.so
      libpcre.so
      libresolv.so
      libnss_nis.so
      libdl.so
      libBrokenLocale.so
      libblkid.so
      libnss_compat.so"

if [[ $# -eq 0 ]]; then
    echo "Missing rootfs path"
    exit 1
fi

rootfs=$1

for lib in $libs
do
    echo "Fix $rootfs/usr/lib/arm-linux-gnueabihf/$lib"
    link_content=$(readlink $rootfs/usr/lib/arm-linux-gnueabihf/$lib)
    rm $rootfs/usr/lib/arm-linux-gnueabihf/$lib
    ln -s ../../..$link_content $rootfs/usr/lib/arm-linux-gnueabihf/$lib
done
