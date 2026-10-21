P=/storage/roms/ports/ragingloop
echo "=== script dir ==="
ls $P/assets/script | head -20
echo "count: $(ls $P/assets/script | wc -l)"
echo "=== ttf/otf under /storage ==="
find /storage -iname '*.ttf' -o -iname '*.otf' 2>/dev/null | head -30
echo "=== ttf/otf under /usr ==="
find /usr -iname '*.ttf' -o -iname '*.otf' 2>/dev/null | head -30
echo "=== fonts dirs ==="
ls -d /usr/share/fonts /storage/.config/fonts 2>/dev/null
echo "=== roms ==="
ls /storage/roms 2>/dev/null
echo "=== ports ==="
ls /storage/roms/ports 2>/dev/null | head -40
