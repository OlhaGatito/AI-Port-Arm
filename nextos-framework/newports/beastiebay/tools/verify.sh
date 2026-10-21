#!/bin/sh
# Post-fix verification: is the frame-3 SIGSEGV gone, and what does the
# managed side do next?
G=/roms/ports/beastiebay
L=$G/log.txt
echo "=== crash / fault ==="
grep -c 'signal 11\|SEGV_MAPERR\|\[D/CRASH\]' $L
grep -n 'signal 11\|SEGV_MAPERR' $L | head -3
echo
echo "=== incomplete method slot (the new fail-closed path) ==="
grep -c 'incomplete method id' $L
grep -n 'incomplete method id' $L | head -5
echo
echo "=== frame progression ==="
grep -n 'frame [0-9]* keep=' $L | tail -12
echo
echo "=== render-loop stop / quit ==="
grep -n 'render-loop stop\|requested lifecycle exit\|nativeDone' $L | tail -5
echo
echo "=== managed error / dialog (Kairosoft code 100) ==="
grep -n 'showDialog\|Application.Quit\|Error\b' $L | tail -10
echo
echo "=== method table high-water mark ==="
grep -o 'new method id [0-9]*' $L | tail -3
grep -c 'new method id' $L
echo
echo "=== Unity managed log (last 15) ==="
grep -n '\[?/Unity\]\|\[D/Unity\]\|\[E/Unity\]\|\[W/Unity\]' $L | tail -15
echo
echo "=== GL / draw evidence ==="
grep -n 'gl:\|GLLOG\|non_black\|screenshot\|frame proof' $L | tail -8
echo
echo "=== tail ==="
tail -12 $L
echo VERIFY_DONE
