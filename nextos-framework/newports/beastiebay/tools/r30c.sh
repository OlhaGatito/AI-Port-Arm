#!/bin/sh
echo "=== does logcat work at all here? ==="
logcat -d 2>&1 | tail -5
echo "--- total lines ---"
logcat -d 2>/dev/null | wc -l
echo
echo "=== is the process still running? ==="
ps -A 2>/dev/null | grep -i beastie
echo
echo "=== the port's own log: real showDialog invocations (with -> ) ==="
grep -n 'showDialog.*->' /roms/ports/beastiebay/log.txt | head
echo
echo "=== JT-traced managed calls, last 40 ==="
grep -n '\[fp2/sdk\]' /roms/ports/beastiebay/log.txt | tail -40
echo R30C_DONE
