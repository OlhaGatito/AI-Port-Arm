#!/bin/sh
G=/roms/ports/beastiebay
L=$G/log.txt
echo "=== FULL CRASH BLOCK (1575-1659) ==="
sed -n '1575,1659p' $L
echo
echo "=== context before crash: lines 1500-1590 (grep interesting) ==="
sed -n '1500,1590p' $L | grep -v '^\[fp2\] jni: new method id' | grep -v 'dlsym(' | head -60
echo CRASH_DONE
