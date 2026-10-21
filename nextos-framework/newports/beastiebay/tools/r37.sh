#!/bin/sh
L=/roms/ports/beastiebay/log.txt
echo "=== modules lines ==="
grep -n 'fp2/modules' $L | head -30
echo "--- count: $(grep -c 'fp2/modules' $L) ---"
echo R37_DONE
