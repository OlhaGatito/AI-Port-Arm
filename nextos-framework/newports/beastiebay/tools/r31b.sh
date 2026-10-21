#!/bin/sh
export FP2_DIALOG_TRACE=1
sh /tmp/bb_launch.sh
echo "--- waiting 55s for the boot sequence to reach the error dialog ---"
sleep 55
sh /tmp/bb_stop.sh
echo
echo "=== the showDialog backtrace ==="
grep -n -A 30 'fp2/trace. showDialog id=0' /roms/ports/beastiebay/log.txt | head -40
echo R31B_DONE
