#!/bin/bash
# Map lr offset into libunity.so .text
set -e
SO=/src/apk/lib/arm64-v8a/libunity.so
echo '=== sections ==='
aarch64-linux-gnu-readelf -S "$SO" | grep -E 'text|plt|rodata' | head
echo '=== .text file offset ==='
aarch64-linux-gnu-readelf -S "$SO" | awk '/\.text/ {print}'
echo '=== dynsym sample ==='
aarch64-linux-gnu-nm -D "$SO" 2>/dev/null | head -5
# From on-device maps: r-xp code at file offset 0x381000 in one mapping
# Typical Unity: executable segment offset
aarch64-linux-gnu-readelf -l "$SO" | grep -A1 'R E'
