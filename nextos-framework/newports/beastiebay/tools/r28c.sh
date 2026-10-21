#!/bin/sh
# run28c: Config.init is being handed an RSA public key, which makes Kairosoft's
# licence check the prime suspect for error 100. Two things decide it:
#   1. the boolean/flag arguments of Config.init (the first is 1, the second 0);
#   2. what the managed side does with the licence state afterwards.
# Also verify the Data bundle really is present, because stat("data.unity3d")
# returning -1 would be an independent hard failure.
D=/roms/ports/beastiebay
L=$D/log.txt
echo "=== assets/bin/Data contents ==="
ls -la $D/assets/bin/Data/ 2>/dev/null | head -30
echo
echo "=== is data.unity3d present under any name? ==="
find $D/assets -name 'data.unity3d*' -o -name '*.unity3d' 2>/dev/null | head
find $D -name 'data.unity3d' 2>/dev/null | head
echo
echo "=== the two stat lines the loader reported, verbatim ==="
grep -n 'data.unity3d' $L | head -5
echo
echo "=== licence / registration related identifiers in the log ==="
grep -in 'licen\|regist\|auth\|Config.init\|purchase\|kairopark\|DEBUG' $L | head -20
echo
echo "=== what the game did with Config: every kairo/Config and Utility call, in order ==="
grep -n 'fp2/sdk\]\|kairo/android/plugin' $L | head -40
echo
echo "=== Scripting object creation order: which Unity singleton got made last ==="
grep -n 'FindClass\|new method id' $L | tail -25
echo R28C_DONE
