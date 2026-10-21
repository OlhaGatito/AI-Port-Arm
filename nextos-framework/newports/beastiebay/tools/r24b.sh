#!/bin/sh
# run24b: the managed side was told the screen is 720x720 (PlayerPrefs
# Screenmanager Resolution 720x720, getScaleRatio(240,240,720,720) -> 3.0).
# Is that what the hardware actually offers?  A mismatch here makes every
# layout computation wrong even though each individual value looks sane.
L=/roms/ports/beastiebay/log.txt
echo "=== device framebuffer / mode ==="
cat /sys/class/graphics/fb0/virtual_size 2>/dev/null || echo "(no fb0 virtual_size)"
cat /sys/class/graphics/fb0/modes 2>/dev/null || echo "(no fb0 modes)"
echo
echo "=== DRM connectors ==="
for c in /sys/class/drm/card*-*/status; do
  [ -f "$c" ] || continue
  echo "$(dirname $c): $(cat $c) modes: $(head -1 $(dirname $c)/modes 2>/dev/null)"
done
echo
echo "=== loader's own view ==="
grep -n 'drawable\|SDL_VIDEODRIVER\|PLATFORM=\|window size\|GL_VERSION\|OpenGL ES' $L | head -12
echo
echo "=== scale ratio / app size as the game saw them ==="
grep -n 'getScaleRatio\|getAppWidth\|getAppHeight\|Display metrics\|Display real size' $L
echo R24B_DONE