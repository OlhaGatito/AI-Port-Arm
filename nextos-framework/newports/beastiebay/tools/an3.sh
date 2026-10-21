#!/bin/sh
G=/roms/ports/beastiebay
L=$G/log.txt
echo "=== frame 2 (1548) -> crash (1587) RAW, no filter ==="
sed -n '1548,1588p' $L
echo
echo "=== all unhandled/miss between line 1200 and 1600 ==="
sed -n '1200,1600p' $L | grep -n 'unhandled\|miss\]\|Exception\|showDialog\|Utility\.\|Tapjoy' | head -40
echo
echo "=== any String/Class/Type null-ish misses whole log ==="
grep -n 'miss\]' $L | grep -i 'string\|class\|type\|name\|null' | head -30
echo CRASH2_DONE
