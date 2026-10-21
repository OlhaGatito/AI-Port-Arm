set -e
echo "=== uname ==="
uname -a
echo "=== mem ==="
grep -E 'MemTotal|MemAvailable' /proc/meminfo
echo "=== payload ==="
P=/storage/roms/ports/ragingloop
ls -d $P
find $P -type f | wc -l
du -sh $P
ls $P
echo "=== key dirs ==="
for d in assets script sound voice background; do
  echo -n "$d: "; find $P -maxdepth 3 -type d -name "$d" | head -3 | tr '\n' ' '; echo
done
echo "=== font ==="
ls -l /storage/roms/ports/*/NotoSansSC-Regular.ttf 2>/dev/null | head -3
echo "=== libs ==="
ls /usr/lib | grep -iE 'SDL2|libogg|libvorbis|libpng|freetype' | head -20
echo "=== sdl ==="
SDL_VIDEODRIVER=dummy /usr/lib/libSDL2-2.0.so.0 2>/dev/null || true
ldd --version 2>/dev/null | head -1
