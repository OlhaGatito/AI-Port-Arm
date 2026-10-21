#!/bin/sh
# Which Config.init actually ran, with what arguments, and what Config.finish
# did.  The RSA public key that rides on Config.init is the licence chain, so
# a missed init is a plausible error-100 source.
L=/roms/ports/beastiebay/log.txt
echo "=== every Config.* call in order ==="
grep -n 'Config\.' $L
echo
echo "=== getAppWidth / getAppHeight / getScaleRatio / getStatusBar in order ==="
grep -n 'getAppWidth\|getAppHeight\|getScaleRatio\|getStatusBar\|isTablet\|getPackageName\|getBatteryInfo\|getClockTime' $L
echo
echo "=== Config block raw context ==="
N=$(grep -n 'plugin/Config.init' $L | head -1 | cut -d: -f1)
sed -n "$((N-6)),$((N+30))p" $L
echo W4_DONE