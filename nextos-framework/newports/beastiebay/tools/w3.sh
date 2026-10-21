#!/bin/sh
# The window that contains the Kairosoft plugin bootstrap: Config.init,
# getAppWidth/getAppHeight, the first AdMobView call.
L=/roms/ports/beastiebay/log.txt
FIRST=$(grep -n 'showDialog(id=0' $L | head -1 | cut -d: -f1)
echo "=== window $((FIRST-345))..$((FIRST-135)) ==="
sed -n "$((FIRST-345)),$((FIRST-135))p" $L
echo
echo "=== every degraded (non-slash-prefixed or short) class name passed to getMethodID ==="
grep -o 'getMethodID([^,]*' $L | sed 's/getMethodID(//' | sort -u | head -60
echo W3_DONE