#!/bin/sh
# run26c: boot.config drives Unity's startup and Kairosoft's BootForm self-check.
# Also enumerate every Kairosoft plugin call that is still falling through to
# the generic stub, because error 100 is a self-check CONDITION and each
# remaining stub is a candidate until proven otherwise.
L=/roms/ports/beastiebay/log.txt
echo "=== boot.config contents (as the managed side sees it) ==="
cat /roms/ports/beastiebay/assets/bin/Data/boot.config 2>/dev/null
echo
echo "=== how the loader fed boot.config to the managed side ==="
grep -n 'boot.config\|Scanner\|hasKey\|nextLine\|next()' $L | head -20
echo
echo "=== ALL remaining unhandled calls, de-duplicated, in first-seen order ==="
grep -n 'unhandled ' $L | sed 's/^\([0-9]*\):.*unhandled /\1 /' | awk '!seen[$2" "$3" "$4" "$5" "$6" "$7" "$8]++'
echo R26C_DONE