#!/bin/sh
# run39: deploy the port binary, launch with FP2_DIALOG_TRACE, collect state log.
G=/roms/ports/beastiebay
echo "=== md5 before ==="
md5sum $G/beastiebay-nextos 2>/dev/null
if [ -f /tmp/bb_newbin ]; then
  cp /tmp/bb_newbin $G/beastiebay-nextos.new
  chmod +x $G/beastiebay-nextos.new
  mv -f $G/beastiebay-nextos.new $G/beastiebay-nextos
  echo "deployed"
fi
md5sum $G/beastiebay-nextos
: > $G/log.txt
FP2_DIALOG_TRACE=1 sh /tmp/bb_launch.sh
sleep 55
for pid in $(ps -eo pid,args | grep -E 'beastiebay-nextos' | grep -v grep | cut -d' ' -f1); do
  kill -9 $pid 2>/dev/null
done
echo "=== state lines ==="
grep -n 'state:' $G/log.txt | head -80
echo "--- state count: $(grep -c 'state:' $G/log.txt) ---"
echo "=== showDialog ==="
grep -n 'showDialog' $G/log.txt | head -5
echo R39RUN_DONE
