#!/bin/sh
L=/roms/ports/beastiebay/log.txt
echo "=== all state:/hook:/il2: lines ==="
grep -n -E 'state:|hook:|il2:' $L | head -30
echo
echo "=== 1405-1420 (where the hook landed) ==="
sed -n '1405,1420p' $L | cut -c1-120
echo
echo "=== 640-700 (failed attempts) ==="
sed -n '640,700p' $L | grep -E 'state:|unity|probe|il2' | cut -c1-120 | head -20
echo
echo "=== does the game reach BootForm at all? search literal 6027 usage ==="
grep -c 'BootForm' $L
echo "=== crash? ==="
grep -c 'D/CRASH\|signal 11' $L
echo "=== tail ==="
tail -5 $L | cut -c1-120
echo R39C_DONE
