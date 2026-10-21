#!/bin/sh
echo "=== last Unity-side game logs:"
grep -n '\[?/Unity\]\|\[D/Unity\]\|\[E/Unity\]\|\[W/Unity\]\|\[V/Unity\]' /roms/ports/beastiebay/log.txt | tail -20
echo "=== last 3 lines before frame 4:"
n=$(grep -n 'frame 4 keep' /roms/ports/beastiebay/log.txt | head -1 | cut -d: -f1)
sed -n "$((n-14)),$((n))p" /roms/ports/beastiebay/log.txt
