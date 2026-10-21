#!/bin/bash
# Probe the portmaster-builder aarch64 container for the toolchain pieces the
# ragingloop port needs. Run mounted at /src.
set -u
echo "-- SDL2 header --"
ls -l /usr/include/SDL2/SDL.h 2>&1 | tail -1
echo "-- static media libs --"
ls /usr/lib/aarch64-linux-gnu/ 2>/dev/null | grep -E '^lib(ogg|vorbis|vorbisfile|mikmod|modplug|png|jpeg|freetype|theora)[^ ]*\.a$'
echo "-- shared media libs --"
ls /usr/lib/aarch64-linux-gnu/ 2>/dev/null | grep -E '^lib(ogg|vorbis|png|jpeg|freetype)[^ ]*\.so' | head -12
echo "-- pkg-config sdl2 --"
pkg-config --modversion sdl2 2>&1 | tail -1
echo "-- freetype header --"
ls /usr/include/freetype2/ft2build.h 2>&1 | tail -1
echo "-- glibc of container --"
ls -l /usr/lib/aarch64-linux-gnu/libc.so.6 2>&1 | tail -1
echo "-- compilers --"
aarch64-linux-gnu-gcc --version | head -1
echo "-- zlib --"
ls /usr/include/zlib.h 2>&1 | tail -1
