#!/bin/sh
# run22: error 100 survived all four run21 fixes, so the cause is elsewhere.
# Print the raw window in front of the dialog with no filtering, plus every
# SDK/managed call in order, so the last thing before the throw is visible.
L=/roms/ports/beastiebay/log.txt
FIRST=$(grep -n 'showDialog(id=0' $L | head -1 | cut -d: -f1)
echo "=== raw window $((FIRST-70))..$((FIRST+16)) ==="
sed -n "$((FIRST-70)),$((FIRST+16))p" $L
echo
echo "=== all fp2/sdk lines in order ==="
grep -n '\[fp2/sdk\]' $L
echo
echo "=== unity log lines ==="
grep -n '\[?/Unity\]\|\[D/Unity\]\|\[E/Unity\]\|\[W/Unity\]\|\[?/AndroidRuntime\]' $L | tail -20
echo R22_DONE