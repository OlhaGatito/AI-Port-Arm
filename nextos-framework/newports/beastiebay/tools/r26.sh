#!/bin/sh
# run26 verification: did the last four contracts bind, and did error 100 go?
L=/roms/ports/beastiebay/log.txt
echo "=== crash / incomplete ==="
grep -c 'signal 11\|SEGV_MAPERR' $L; grep -c 'incomplete method id' $L
echo "=== frames ==="
grep -n 'frame [0-9]* keep=\|render-loop stop' $L | tail -8
echo "=== run26 contracts bound? ==="
grep -n 'getNotificationFilter\|fullscreen(\|createAdMobView\|removeAdMobView\|getNotificationData' $L | head -16
echo "=== any of them still unhandled? ==="
grep -c 'unhandled kairo/android/plugin' $L
grep -n 'unhandled kairo/android/plugin' $L | head -10
echo "=== error 100? ==="
grep -c 'An error has occurred' $L
grep -n 'showDialog(id=' $L
echo "=== managed exceptions (should stay 0) ==="
grep -c 'fp2/il2' $L
echo "=== draw evidence ==="
grep -n 'draw #\|frame proof\|GRAPHICS-EVIDENCE' $L | tail -3
echo R26_DONE