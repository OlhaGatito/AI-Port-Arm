#!/bin/sh
L=/roms/ports/beastiebay/log.txt
echo "=== state: hook install ==="
grep -n 'state:' $L | head -60
echo "--- total state lines: $(grep -c 'state:' $L) ---"
echo
echo "=== dialog context ==="
grep -n 'Utility.showDialog' $L | head -10
echo R39_DONE
