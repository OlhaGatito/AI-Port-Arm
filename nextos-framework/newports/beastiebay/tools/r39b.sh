#!/bin/sh
L=/roms/ports/beastiebay/log.txt
echo "=== 1795-1840 (hook install vs showDialog ordering) ==="
sed -n '1795,1840p' $L
echo "=== all hook/state lines with line numbers ==="
grep -n -E 'state:|hook:' $L
echo "=== first 40 lines (what runs before) ==="
sed -n '1,40p' $L | cut -c1-140
echo R39B_DONE
