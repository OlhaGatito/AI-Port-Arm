echo "== du of /storage (same fs only) =="
du -x -d 1 -h /storage 2>/dev/null | sort -k1 -h | tail -20
echo "== dirs modified in last 6h (same fs) =="
find /storage -xdev -maxdepth 3 -type d -mmin -360 2>/dev/null | head -30
echo "== voice count anywhere on device fs =="
find /storage -xdev -type d -name voice 2>/dev/null | head
echo "== ogg files on /storage fs =="
find /storage -xdev -name '*.ogg' 2>/dev/null | wc -l
echo "== swapfile =="
ls -la /storage/.cache/swapfile 2>/dev/null
swapon --show 2>/dev/null | head
