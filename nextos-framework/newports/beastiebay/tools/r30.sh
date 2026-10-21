#!/bin/sh
L=/roms/ports/beastiebay/log.txt
echo "=== VERDICT: showDialog(id=0) count ==="
grep -c 'showDialog' $L
echo
echo "=== root mentions in port stdout ==="
grep -n -i 'root' $L | head -10
echo
echo "=== top callers, by frequency ==="
grep -o '[a-zA-Z0-9_/$.]*->' $L | sort | uniq -c | sort -rn | head -30
echo
echo "=== window before showDialog ==="
FIRST=$(grep -n 'showDialog' $L | head -1 | cut -d: -f1)
if [ -n "$FIRST" ]; then
  N=$((FIRST>70 ? FIRST-70 : 1))
  sed -n "${N},$((FIRST+2))p" $L
else
  echo NO_SHOWDIALOG
  tail -40 $L
fi
echo R30_DONE
