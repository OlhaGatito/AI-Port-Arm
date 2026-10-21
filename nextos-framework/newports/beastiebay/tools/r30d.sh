#!/bin/sh
L=/roms/ports/beastiebay/log.txt
echo "=== lines 1600-1680: getScaleRatio and friends ==="
sed -n '1600,1680p' $L
echo
echo "=== lines 1780,1840: the two showDialog calls in full ==="
sed -n '1780,1840p' $L
echo R30D_DONE
