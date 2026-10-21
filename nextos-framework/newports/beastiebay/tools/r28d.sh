#!/bin/sh
# run28d: licensing/IAP is now EXCLUDED as the cause -- dex shows the checker
# classes exist, but the device log shows getLicenseCheck/startLicenseCheck/
# billing_ are never called at all (no Play Services, so Kairosoft skips them).
# Config.init only stores the key.
#
# So go back to the hard criterion: the last value that is actually WRONG
# before showDialog(id=0). run28b showed these two lines and nothing was done
# about them:
#     stat("assets/bin/Data/data.unity3d") -> -1
#     stat("assets/bin/Data/globalgamemanagers.resG") -> -1
# Unity's data bundle is missing. In a real APK it lives at
# apk/assets/bin/Data/data.unity3d; verify whether the extraction dropped it or
# whether this Unity build simply does not use that name.
L=/roms/ports/beastiebay/log.txt
D=/roms/ports/beastiebay
echo "=== all stat() lines for assets/bin/Data, in order ==="
grep -n 'stat("assets' $L
echo
echo "=== the resource files the game actually opened (stat -> 0) ==="
grep -n 'stat("assets' $L | grep '\-> 0' | tail -20
echo
echo "=== globalgamemanagers present on device? ==="
ls -la $D/assets/bin/Data/globalgamemanagers* 2>/dev/null
echo
echo "=== count of hashed bundles on device ==="
ls $D/assets/bin/Data/ | wc -l
echo
echo "=== boot.config (tells Unity which scene/level to load) ==="
cat $D/assets/bin/Data/boot.config 2>/dev/null
echo
echo "=== what did Unity log about its own data files? ==="
grep -n -i 'unity\|scene\|level\|bundle\|serialized' $L | head -25
echo R28D_DONE
