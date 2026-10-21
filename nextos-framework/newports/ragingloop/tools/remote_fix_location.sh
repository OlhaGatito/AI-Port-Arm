set -e
BAD='/storage/D:/Programs/Git/storage/roms/ports/ragingloop'
GOOD=/storage/roms/ports/ragingloop
JUNK=/storage/D:
echo "== before =="
find "$BAD" -type f | wc -l
du -sh "$BAD"
ls "$BAD"
echo "== move =="
if [ -e "$GOOD" ]; then echo "TARGET ALREADY EXISTS - abort"; exit 1; fi
mkdir -p /storage/roms/ports
mv "$BAD" "$GOOD"
echo "== after =="
find "$GOOD" -type f | wc -l
du -sh "$GOOD"
ls "$GOOD"
echo "== junk tree: only empty dirs may remain =="
LEFT=$(find "$JUNK" -type f | wc -l)
echo "files left under $JUNK = $LEFT"
if [ "$LEFT" = "0" ]; then
  rm -rf "$JUNK"
  echo "removed $JUNK"
else
  echo "NOT removing - unexpected content"
  find "$JUNK" -type f | head -10
fi
ls -la /storage | grep -E 'D:|roms' || true
echo "== space =="
df -h /storage /storage/roms | sed -n '1,4p'
