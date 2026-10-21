#!/bin/sh
G=/roms/ports/beastiebay
echo "=== stage the new binary as .new, then swap atomically ==="
cp $G/beastiebay-nextos $G/beastiebay-nextos.prev 2>/dev/null
cp /tmp/beastiebay-nextos.new $G/beastiebay-nextos.new
chmod 755 $G/beastiebay-nextos.new
mv -f $G/beastiebay-nextos.new $G/beastiebay-nextos
sync
echo "remote md5:"; md5sum $G/beastiebay-nextos
echo
export FP2_DIALOG_TRACE=1
sh /tmp/bb_launch.sh
echo "--- waiting 55s ---"
sleep 55
sh /tmp/bb_stop.sh
echo
echo "=== the backtrace, now with module offsets ==="
grep -n -A 8 'fp2/trace. showDialog id=0' $G/log.txt | head -14
echo R32_DONE
