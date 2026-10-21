echo "=== uptime ==="
uptime
cat /proc/uptime
echo "=== run log ==="
tail -60 /tmp/rl_run.log 2>/dev/null
echo "=== snap ==="
ls -l /tmp/rl_snap.bmp 2>/dev/null
echo "=== procs ==="
ps -eo pid,comm | grep -iE 'emulationstation|ragingloop' | head
echo "=== dmesg tail ==="
dmesg 2>/dev/null | tail -15
