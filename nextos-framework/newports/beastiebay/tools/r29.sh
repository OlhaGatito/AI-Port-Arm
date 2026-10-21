#!/bin/sh
# run29: the 98 previously-unbound Kairosoft contracts are now bound. Which of
# them does the start-up self-check actually consult, and did error 100 move?
L=/roms/ports/beastiebay/log.txt
echo "=== VERDICT FIRST: is the error dialog still there? ==="
grep -c 'showDialog(id=0' $L
grep -n 'showDialog' $L | head -6
echo
echo "=== run29 new contracts, in call order ==="
grep -n 'Utility.isTablet\|Utility.isDebug\|Utility.getWidth\|Utility.getHeight\|Utility.isFullscreen\|Utility.fileExists\|Utility.hasPermission\|Utility.getErrMsg\|Utility.getSignature\|Utility.hasIntent\|Utility.checkPlayServices\|Utility.getLicenseCheck\|StatusBarHeight\|SystemBarHeight' $L | head -30
echo
echo "=== Config.getParam / setParam (newly bound) ==="
grep -n 'Config.getParam\|Config.setParam' $L | head -20
echo
echo "=== the generic-rest handler ==="
grep -c 'no such service on this device' $L
grep -n 'no such service on this device' $L | head -10
echo
echo "=== remaining unhandled calls (should be much smaller now) ==="
grep -n 'unhandled ' $L | sed 's/^\([0-9]*\):.*unhandled /\1 /' | awk '!seen[$0]++'
echo
echo "=== last 25 lines before showDialog(id=0), if any ==="
FIRST=$(grep -n 'showDialog(id=0' $L | head -1 | cut -d: -f1)
if [ -n "$FIRST" ]; then
  sed -n "$((FIRST>25 ? FIRST-25 : 1)),$((FIRST+3))p" $L
else
  echo "(NO showDialog(id=0) -- the self-check passed. Tail follows.)"
  tail -30 $L
fi
echo R29_DONE
