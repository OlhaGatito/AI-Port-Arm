#!/bin/sh
# Runs INSIDE the aarch64 PortMaster builder image (invoked by tools/build.sh).
# The image carries aarch64 libvorbis/libogg/libvorbisfile, and the device has
# the matching .so.0 files in /usr/lib, so audio links against the system libs
# rather than bundling anything.
set -e
cd /src

CFLAGS="-O2 -g -std=gnu11 -Wall -Wextra -Wno-unused-parameter -DNDEBUG"
OBJS="main res img txt scr audio"
AUDIO=`aarch64-linux-gnu-pkg-config --cflags --libs vorbisfile`

mkdir -p build-aarch64
for m in $OBJS; do
	aarch64-linux-gnu-gcc $CFLAGS -I/src/vendor -c src/$m.c -o build-aarch64/$m.o
done

aarch64-linux-gnu-gcc -o build-aarch64/ragingloop build-aarch64/*.o \
	-lSDL2 $AUDIO -lm -lpthread -ldl -static-libgcc

echo "--- file ---"
file build-aarch64/ragingloop
echo "--- NEEDED ---"
aarch64-linux-gnu-readelf -d build-aarch64/ragingloop | grep NEEDED
echo "--- GLIBC versions referenced ---"
aarch64-linux-gnu-readelf -V build-aarch64/ragingloop | grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -5
if [ "${RL_STRIP:-0}" = 1 ]; then aarch64-linux-gnu-strip build-aarch64/ragingloop; fi
ls -l build-aarch64/ragingloop
