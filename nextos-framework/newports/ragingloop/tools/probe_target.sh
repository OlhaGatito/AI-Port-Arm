#!/bin/sh
# ROCKNIX target probe for the ragingloop port. ASCII-only output.
echo "== uname/arch =="
uname -a
grep -m1 -c processor /proc/cpuinfo | sed 's/^/cpus=/'
echo "== memory =="
head -3 /proc/meminfo
echo "== display mode (DRM) =="
for f in /sys/class/drm/card0-*/status /sys/class/drm/card0-*/modes; do
  [ -e "$f" ] || continue
  printf '%s: ' "$f"; head -c 200 "$f" | tr '\n' ' '; echo
done 2>/dev/null | head -12
echo "== fonts: CJK candidates =="
find /usr/share/fonts /storage/roms -iname '*.ttf' -o -iname '*.otf' -o -iname '*.ttc' 2>/dev/null | head -40
echo "-- grep for cjk/noto names --"
find / -maxdepth 6 -iname '*cjk*' -o -iname '*noto*' 2>/dev/null | grep -i -E 'ttf|otf|ttc|font' | head -20
echo "== GL libs =="
ls -l /usr/lib/libmali* 2>/dev/null | head -8
ls /usr/lib | grep -E '^(libEGL|libGLES|libGL|libglapi)' | head -12
ls /usr/share/glvnd/egl_vendor.d 2>/dev/null
echo "== GLES1 header/lib presence =="
ls /usr/lib | grep -i -E 'GLESv1|v1_CM' | head
echo "== media libs =="
ls /usr/lib | grep -i -E 'vorbis|ogg|mpg123|SDL2_(mixer|image|ttf|gfx)|freetype|png|jpeg|zstd' | head -20
echo "== SDL2 =="
ls -l /usr/lib/libSDL2* 2>/dev/null | head -5
echo "== audio stack =="
pgrep -a pipewire 2>/dev/null | head -3
pgrep -a pulseaudio 2>/dev/null | head -3
ls /usr/lib | grep -E 'libasound|libpipewire|libpulse' | head -8
echo "== storage =="
df -h /storage /storage/roms 2>/dev/null
mount | grep -E 'roms|storage' | head -5
echo "== ports layout =="
ls /storage/roms/ports | head -20
echo "-- ports_scripts? --"
ls -d /storage/roms/ports_scripts 2>/dev/null || echo "no ports_scripts (merged layout)"
echo "== tools on device =="
for t in gcc make cmake git ffmpeg curl wget python3 gdb dosbox; do
  printf '%-8s ' "$t"; command -v $t || echo MISSING
done
echo "== services =="
systemctl is-running essway 2>/dev/null; systemctl list-units --no-pager 2>/dev/null | grep -iE 'essway|emulation|kms' | head -5
echo PROBE_DONE
