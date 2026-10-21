#!/bin/sh
L=/roms/ports/beastiebay/log.txt
echo "=== crash / incomplete ==="
grep -c 'signal 11\|SEGV_MAPERR' $L; grep -c 'incomplete method id' $L
echo "=== frames ==="
grep -n 'frame [0-9]* keep=' $L | tail -8
echo "=== GL extensions cache + glGetString ==="
grep -n 'GL_EXTENSIONS=\|glGetString' $L
echo "=== display metrics / real size ==="
grep -n 'Display metrics\|Display real size' $L
echo "=== error 100? ==="
grep -c 'An error has occurred' $L
grep -n 'showDialog(id=' $L
echo "=== texture warning ==="
grep -c 'out of range' $L
echo "=== last sdk calls before any dialog ==="
grep -n '\[fp2/sdk\]' $L | tail -12
echo "=== draw evidence ==="
grep -n 'draw #\|non_black\|frame proof' $L | tail -5
echo V3_DONE
