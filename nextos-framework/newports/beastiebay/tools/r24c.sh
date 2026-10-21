#!/bin/sh
# run24c: every device-side value the managed side reads now matches reality
# (720x720 native panel, correct dpi/density, correct package name, real GL
# extensions containing GL_OES_depth24).  Kairosoft error 100 is its GENERIC
# startup failure code, so the remaining candidates are the things that are not
# values at all.  Check what the boot tried to reach and whether it could.
L=/roms/ports/beastiebay/log.txt
echo "=== CONNECTIVITY / permissions ==="
grep -n -i 'connect\|permission\|INTERNET\|network\|wifi\|socket' $L | head -25
echo
echo "=== does the device have network at all? ==="
ip route 2>/dev/null | head -5
echo "--- dns ---"
cat /etc/resolv.conf 2>/dev/null | head -5
echo "--- reach kairopark (5s) ---"
ping -c1 -W3 kairopark.jp 2>&1 | tail -3
echo
echo "=== System.loadLibrary calls ==="
grep -n 'loadLibrary\|System.load(' $L
echo
echo "=== any exception-ish Unity output at all ==="
grep -n -i 'exception\|error\|fail\|assert' $L | grep -iv 'no error' | head -25
echo R24C_DONE