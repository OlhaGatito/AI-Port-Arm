#!/bin/sh
# run25b: the hooks installed but ZERO managed exceptions fired, while
# showDialog(id=0) still did.  That rules out the exception path entirely and
# means Kairosoft REPORTS the error deliberately.  Confirm the count and see
# what the managed side was doing at the moment it decided.
L=/roms/ports/beastiebay/log.txt
echo "=== every [fp2/il2] line (hook install vs actual throw) ==="
grep -n 'fp2/il2' $L
echo
echo "=== total lines / frames ==="
wc -l $L
grep -n 'frame [0-9]* keep=' $L | tail -8
echo
echo "=== the last 45 JNI calls before showDialog(id=0), unfiltered ==="
FIRST=$(grep -n 'showDialog(id=0' $L | head -1 | cut -d: -f1)
sed -n "$((FIRST-45)),$((FIRST))p" $L
echo R25B_DONE