#!/bin/sh
echo "=== identity ==="
uname -a
cat /etc/os-release 2>/dev/null | head -3
echo "=== uptime / load ==="
uptime
echo "=== memory ==="
free -m | head -2
echo "=== gpu ==="
ls /usr/local/lib/aarch64-linux-gnu/ 2>/dev/null | grep -i mali
echo "=== panel ==="
cat /sys/class/graphics/fb0/virtual_size 2>/dev/null
echo "=== ports layout ==="
ls -d /roms/ports 2>/dev/null
ls -d /roms/ports_scripts 2>/dev/null || echo "no ports_scripts (ArkOS single-dir)"
echo "=== deployed game dir ==="
ls -la /roms/ports/beastiebay/ 2>/dev/null | head -12
echo "=== launcher ==="
ls -la "/roms/ports/Beastie Bay.sh" 2>/dev/null
echo "=== deployed binary vs local ==="
ls -la /roms/ports/beastiebay/beastiebay-nextos 2>/dev/null
sha256sum /roms/ports/beastiebay/beastiebay-nextos 2>/dev/null
echo "=== last log tail ==="
tail -5 /roms/ports/beastiebay/log.txt 2>/dev/null
echo "=== free space ==="
df -h /roms | tail -1
echo PROBE_DONE
