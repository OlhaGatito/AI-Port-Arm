#!/bin/sh
L=/roms/ports/beastiebay/log.txt
echo "=== crash count / incomplete slots ==="
grep -c 'signal 11\|SEGV_MAPERR' $L
grep -c 'incomplete method id' $L
echo "=== frame progression ==="
grep -n 'frame [0-9]* keep=' $L | tail -8
echo "=== GLES20 glGetString results ==="
grep -n 'glGetString' $L
echo "=== error 100 still there? ==="
grep -c 'An error has occurred' $L
grep -n 'showDialog(id=' $L
echo "=== Display real size ==="
grep -n 'Display real size' $L
echo "=== GL strings from video init ==="
grep -n 'GL_VENDOR=\|GL_RENDERER=\|GL_VERSION=' $L
echo "=== texture warning ==="
grep -n 'out of range' $L
echo "=== tail ==="
tail -6 $L
echo V2_DONE
