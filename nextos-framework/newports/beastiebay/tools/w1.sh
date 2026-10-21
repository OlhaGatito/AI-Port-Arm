#!/bin/sh
# Narrow the causal window: everything from the last SDK call before the
# managed error dialog up to the dialog itself, with no grep filtering, so the
# ordering that matters (which call is last before showDialog) is visible.
L=/roms/ports/beastiebay/log.txt
FIRST=$(grep -n 'showDialog(id=0' $L | head -1 | cut -d: -f1)
echo "=== dialog at line $FIRST; window $((FIRST-140))..$((FIRST+20)) ==="
sed -n "$((FIRST-140)),$((FIRST+20))p" $L
echo WINDOW_DONE