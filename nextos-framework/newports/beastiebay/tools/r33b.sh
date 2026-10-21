#!/bin/sh
L=/roms/ports/beastiebay/log.txt
echo "=== crash backtrace ==="
grep -n -B4 -A 14 'D/CRASH' $L | head -40
echo R33B_DONE
