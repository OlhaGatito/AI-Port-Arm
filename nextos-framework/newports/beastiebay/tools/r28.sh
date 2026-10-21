#!/bin/sh
# run28: verify the run27 parsing primitives (Integer.parseInt, Long.parseLong,
# String.length/isEmpty/trim) actually return real values, and check whether the
# self-check verdict changed. Audio sample rate is the leading indicator: with
# parseInt answering 0, the device was being configured for 0 Hz.
L=/roms/ports/beastiebay/log.txt
echo "=== build id ==="
grep -m1 'build id\|BuildID' $L
echo
echo "=== run27 primitives: parseInt / parseLong ==="
grep -n 'parseInt\|parseLong' $L | head -20
echo
echo "=== String primitives ==="
grep -n 'java/lang/String.length\|String.isEmpty\|String.trim' $L | head -10
echo
echo "=== audio chain: getProperty -> parseInt -> device config ==="
grep -n 'OUTPUT_SAMPLE_RATE\|OUTPUT_FRAMES_PER_BUFFER\|AudioManager\|audio' $L | head -20
echo
echo "=== notification primitives ==="
grep -n 'setNotificationBackground\|cancelNotifications' $L | head -6
echo
echo "=== VERDICT: dialog count and payload ==="
grep -c 'showDialog(id=0' $L
grep -n 'showDialog' $L | head -6
echo
echo "=== any managed exception at all ==="
grep -c 'fp2/il2.*EXC' $L
echo
echo "=== ALL remaining unhandled calls, de-duplicated, in first-seen order ==="
grep -n 'unhandled ' $L | sed 's/^\([0-9]*\):.*unhandled /\1 /' | awk '!seen[$0]++'
echo
echo "=== last 40 lines before the first showDialog(id=0) ==="
FIRST=$(grep -n 'showDialog(id=0' $L | head -1 | cut -d: -f1)
if [ -n "$FIRST" ]; then
  sed -n "$((FIRST>40 ? FIRST-40 : 1)),$((FIRST))p" $L
else
  echo "(no showDialog(id=0) -- the self-check passed?)"
  tail -60 $L
fi
echo R28_DONE
