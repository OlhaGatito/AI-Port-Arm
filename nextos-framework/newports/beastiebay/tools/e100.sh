#!/bin/sh
L=/roms/ports/beastiebay/log.txt
echo "=== 1735-1802 full raw (the real error-100 window) ==="
sed -n '1735,1802p' $L | grep -v longValue
echo E100_DONE
