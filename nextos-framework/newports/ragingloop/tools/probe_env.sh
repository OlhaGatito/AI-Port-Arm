#!/bin/sh
# Read-only environment survey for a new port. ASCII output only (Windows console
# mangles UTF-8 labels).
echo "=== identity ==="
uname -a
cat /etc/os-release 2>/dev/null | head -5
cat /proc/device-tree/compatbile 2>/dev/null | tr '\0' ' '
echo
cat /proc/device-tree/model 2>/dev/null | tr '\0' ' '
echo
echo "=== cpu ==="
nproc
grep -m1 "model name\|Hardware\|Processor" /proc/cpuinfo 2>/dev/null
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null
echo "=== memory ==="
free -m
echo "=== storage ==="
df -h /storage 2>/dev/null
stat -f -c "%T" /storage/roms 2>/dev/null || mount | grep -i "roms"
ls -d /storage/roms/ports /storage/roms/ports_scripts /roms/ports 2>/dev/null
echo "--- ports present ---"
ls /storage/roms/ports 2>/dev/null | head -20
echo "--- PortMaster control.txt ---"
for p in /storage/roms/ports/PortMaster /roms/ports/PortMaster /opt/system/Tools/PortMaster /opt/tools/PortMaster "$HOME/.local/share/PortMaster"; do
  [ -d "$p" ] && echo "DIR $p"
  [ -f "$p/control.txt" ] && echo "CONTROL $p/control.txt"
done
echo "=== graphics ==="
ls /dev/dri 2>/dev/null
ls -d /usr/lib/libmali* 2>/dev/null
ls /usr/lib/libmali 2>/dev/null | head -10
ls /usr/share/glvnd/egl_vendor.d /etc/glvnd/egl_vendor.d 2>/dev/null
for so in libEGL.so.1 libGLESv2.so.2 libGLESv1_CM.so.1 libGL.so.1 libglapi.so.0; do
  found=$(ls /usr/lib/$so 2>/dev/null)
  echo "$so -> ${found:-MISSING}"
done
echo "=== sdl2 / audio ==="
ls /usr/lib/libSDL2-2.0.so* /usr/lib/libSDL2*.so* 2>/dev/null | head -5
ls /usr/lib/libasound.so* /usr/lib/libpulse.so* /usr/lib/libpipewire* 2>/dev/null | head -6
pgrep -a pipewire 2>/dev/null | head -3
pgrep -a pulseaudio 2>/dev/null | head -3
echo "=== windowing ==="
echo "WAYLAND_DISPLAY=$WAYLAND_DISPLAY DISPLAY=$DISPLAY XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR"
ls /usr/lib/libwayland-client.so* 2>/dev/null | head -2
ls /usr/lib/libSDL2* /usr/lib32/libSDL2* 2>/dev/null | head -8
echo "=== toolchain / helpers ==="
for t in python3 ffmpeg gdb curl wget tar unzip git make gcc aarch64-linux-gnu-gcc sdl2-config; do
  v=$(command -v $t 2>/dev/null)
  echo "$t -> ${v:-absent}"
done
ldd --version 2>/dev/null | head -1
echo "=== front end ==="
systemctl list-units --type=service --state=running 2>/dev/null | head -12
echo DONE
