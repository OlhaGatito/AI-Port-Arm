#!/bin/sh
# run28e: BootForm's self-check is InitApp + ClassLoad + four RESLOAD_SEB_*
# presence tests (WAVE / PARTS / KAIRO / MAN, per the metadata name table).
# Error 100 is whichever one of those judged false. So: (a) confirm the four
# resource families actually exist on disk, and (b) capture showDialog's
# listener argument, which is the callback the game uses to decide whether a
# Retry is even possible.
L=/roms/ports/beastiebay/log.txt
D=/roms/ports/beastiebay
echo "=== showDialog with listener argument (both occurrences) ==="
grep -n -A2 'showDialog(id=' $L
echo
echo "=== RESLOAD resource families on disk ==="
echo "-- .unity3d bundles (PARTS/KAIRO/MAN) count:"
ls $D/assets/bin/Data/*.unity3d 2>/dev/null | wc -l
find $D/assets -name '*.unity3d' 2>/dev/null | head -5
echo "-- StreamingAssets / .bytes / .txt (MAN) :"
find $D/assets -name '*.txt' -o -name '*.bytes' 2>/dev/null | head -10
echo "-- wave/sound resources:"
find $D/assets -iname '*wave*' -o -iname '*.wav' -o -iname '*.ogg' 2>/dev/null | head -10
echo
echo "=== assets tree top level ==="
ls -la $D/assets/ 2>/dev/null
echo
echo "=== assets/bin/Data non-hashed files (the ones Unity knows by name) ==="
ls $D/assets/bin/Data/ | grep -v '^[0-9a-f]\{32\}$' | head -20
echo
echo "=== ClassLoad / InitApp related calls in the log ==="
grep -n -i 'class\|InitApp\|LoadBox\|RESLOAD' $L | head -15
echo
echo "=== getParam values the game read (what it decided about the environment) ==="
grep -n 'getParam' $L | head -30
echo R28E_DONE
