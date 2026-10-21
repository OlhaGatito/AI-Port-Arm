#!/bin/sh
# run24: the showDialog backtrace proved the call arrives through libunity's
# JNI bridge (return address below JNI_OnLoad), not from IL2CPP, so the
# decision was made earlier and only surfaces here.
L=/roms/ports/beastiebay/log.txt
echo "=== every stat() that failed (-1) ==="
grep -n 'stat(.*) -> -1' $L
echo
echo "=== files opened under bin/Data, unique ==="
grep -o 'stat("assets/bin/Data/[^"]*") -> 0' $L | sort -u
echo
echo "=== SharedPreferences traffic ==="
grep -n 'SharedPreferences' $L | head -30
echo
echo "=== asset bundle / scene / level evidence ==="
grep -n -i 'assetbundle\|loadscene\|scene\|level\|resources' $L | head -20
echo R24_DONE