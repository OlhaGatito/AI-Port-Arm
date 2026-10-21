#!/bin/sh
# run21 verification: did the four fixes land, and did error 100 go away?
L=/roms/ports/beastiebay/log.txt
echo "=== crash / incomplete ==="
grep -c 'signal 11\|SEGV_MAPERR' $L; grep -c 'incomplete method id' $L
echo "=== frames ==="
grep -n 'frame [0-9]* keep=\|render-loop stop' $L | tail -8
echo "=== fix 1: package name ==="
grep -n 'ballatergames\|getPackageName' $L | head -5
echo "=== fix 2: getAppWidth / getAppHeight ==="
grep -n 'getAppWidth\|getAppHeight' $L | head -6
echo "=== fix 3: GLES20 constants + glGetString ==="
grep -n 'GL_EXTENSIONS\|glGetString' $L
echo "=== fix 4: setParam overwrite / getParam ==="
grep -n 'setParam\|getParam' $L | head -12
echo "=== Config short-name binding ==="
grep -n 'plugin/Config' $L
echo "=== error 100? ==="
grep -c 'An error has occurred' $L
grep -n 'showDialog(id=' $L
echo "=== texture warning ==="
grep -c 'out of range' $L
echo "=== draw evidence ==="
grep -n 'draw #\|frame proof' $L | tail -4
echo R21_DONE