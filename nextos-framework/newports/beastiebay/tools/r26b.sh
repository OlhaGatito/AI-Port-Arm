#!/bin/sh
# run26b: the metadata string table around "root check" also holds
# "first_boot", "BootForm" and the "[Error] 起動できません" message, i.e.
# Kairosoft's boot self-check has a ROOT DETECTION branch.  This port runs as
# root, so see what the managed side could have observed.
L=/roms/ports/beastiebay/log.txt
echo "=== uid / root evidence ==="
id
echo "--- su / magisk / supersu paths ---"
for p in /system/app/Superuser.apk /system/xbin/su /system/bin/su /sbin/su \
         /system/xbin/daemonsu /data/adb/magisk /data/local/su /system/app/SuperSU \
         /system/bin/.ext/.su /system/framework/XposedBridge.jar; do
  [ -e "$p" ] && echo "PRESENT: $p"
done
echo "--- which su ---"
which su 2>/dev/null || echo "(no su on PATH)"
echo "--- test-keys / ro.build.tags ---"
getprop ro.build.tags 2>/dev/null
getprop ro.debuggable 2>/dev/null
getprop ro.secure 2>/dev/null
echo
echo "=== does the log mention root/su at all? ==="
grep -n -i 'root\|/su\b\|supersu\|magisk' $L | head -15
echo
echo "=== boot / first_boot keys the managed side touched ==="
grep -n -i 'first_boot\|boot' $L | head -15
echo R26B_DONE