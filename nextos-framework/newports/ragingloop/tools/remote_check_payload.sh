set -e
R=/storage/roms/ports/ragingloop
echo "== counts =="
find $R -type f | wc -l
du -sh $R
echo "== top =="
ls $R
echo "== assets dirs =="
ls $R/assets
echo "== files =="
ls $R/files 2>/dev/null || echo "(none)"
echo "== save =="
find $R/save -type f 2>/dev/null | head
echo "== spot check =="
ls $R/assets/background 2>/dev/null | wc -l
ls $R/files/voice 2>/dev/null | wc -l
echo "== free =="
df -h /storage | tail -1
