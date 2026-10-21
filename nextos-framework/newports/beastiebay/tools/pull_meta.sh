#!/bin/sh
# Pull the IL2CPP payload for offline analysis. Error 100 is a managed
# self-check verdict, so the condition that produces it only exists in
# global-metadata.dat + libil2cpp.so -- the device log can show which JNI call
# was last, but never what the game decided was wrong.
set -e
D=/roms/ports/beastiebay
mkdir -p /tmp/bbmeta
cp $D/assets/bin/Data/Managed/Metadata/global-metadata.dat /tmp/bbmeta/
cp $D/lib/libil2cpp.so /tmp/bbmeta/
ls -la /tmp/bbmeta/
md5sum /tmp/bbmeta/* 2>/dev/null || md5 /tmp/bbmeta/*
echo PULL_DONE
