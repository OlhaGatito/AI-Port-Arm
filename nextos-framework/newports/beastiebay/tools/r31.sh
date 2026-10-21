#!/bin/sh
echo "=== push launch+stop helpers ==="
cp /roms/ports/beastiebay/tools/bb_launch.sh /tmp/bb_launch.sh 2>/dev/null || echo "(no tools dir on device; using existing /tmp)"
ls -la /tmp/bb_launch.sh /tmp/bb_stop.sh 2>&1
echo R31_DONE
