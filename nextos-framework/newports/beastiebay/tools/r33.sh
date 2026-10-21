#!/bin/sh
L=/roms/ports/beastiebay/log.txt
echo "=== lines: $(wc -l < $L) ==="
echo "=== managed: lines ==="
grep -n 'managed:' $L | head -40
echo
echo "=== showDialog occurrences ==="
grep -n 'showDialog\|fp2/trace' $L | head -12
echo
echo "=== tail ==="
tail -12 $L
echo R33_DONE
