#!/bin/sh
# Self-contained bring-up cycle: upload binaries if requested by caller is
# done separately; this runs launch->wait->stop->collect on the device.
rm -f /tmp/bb_result
sh /tmp/bb_launch.sh > /tmp/bb_launch.log 2>&1
sleep 50
sh /tmp/bb_stop.sh >> /tmp/bb_launch.log 2>&1
{
  echo "=== shots:"
  ls -la /roms/ports/beastiebay/shot* 2>/dev/null
  echo "=== exceptions:"
  grep -c 'Exception' /roms/ports/beastiebay/log.txt
  echo "=== quit/stop lines:"
  grep -E 'render-loop stop|requested lifecycle exit|frame 900|screenshot' /roms/ports/beastiebay/log.txt | tail -6
  echo "=== probes:"
  grep -E 'non_black' /roms/ports/beastiebay/log.txt | tail -3
} > /tmp/bb_result 2>&1
echo DONE >> /tmp/bb_result
