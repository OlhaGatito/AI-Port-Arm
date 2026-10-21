#!/bin/sh
# Self-contained bring-up cycle on the ArkOS handheld.
rm -f /tmp/bb_result
sh /tmp/bb_launch.sh > /tmp/bb_launch.log 2>&1
sleep 50
sh /tmp/bb_stop.sh >> /tmp/bb_launch.log 2>&1
{
  echo "=== shot files:"
  ls -la /roms/ports/beastiebay/shot* 2>/dev/null || echo "(none)"
  echo "=== log size:"
  wc -l /roms/ports/beastiebay/log.txt
  echo "=== DONE"
} > /tmp/bb_result 2>&1
echo DONE >> /tmp/bb_result
