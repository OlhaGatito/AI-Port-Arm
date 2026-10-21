#!/bin/sh
# run27: verify the getMetrics erased-signature forward landed, and see the last
# two unhandled plugin calls before the dialog.
L=/roms/ports/beastiebay/log.txt
echo "=== getMetrics forwarding (run26 fix) ==="
grep -n 'getMetrics\|Display metrics' $L | head -12
echo
echo "=== AudioManager.getProperty (run26 fix) ==="
grep -n 'getProperty\|parseInt' $L | head -10
echo
echo "=== android int/string constants ==="
grep -n 'android int constant' $L | head -12
echo
echo "=== permission constants ==="
grep -n 'checkCallingOrSelfPermission\|PERMISSION' $L | head -8
echo
echo "=== last 30 lines before showDialog(id=0) ==="
FIRST=$(grep -n 'showDialog(id=0' $L | head -1 | cut -d: -f1)
sed -n "$((FIRST-30)),$((FIRST))p" $L
echo R27_DONE