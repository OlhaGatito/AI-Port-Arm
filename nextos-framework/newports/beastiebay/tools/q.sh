#!/bin/sh
n=$(grep -n 'render-loop stop' /roms/ports/beastiebay/log.txt | head -1 | cut -d: -f1)
echo "stop at line $n"
sed -n "$((n-30)),$((n+3))p" /roms/ports/beastiebay/log.txt
