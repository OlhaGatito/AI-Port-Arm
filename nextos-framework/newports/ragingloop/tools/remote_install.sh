set -e
G=/storage/roms/ports/ragingloop
mkdir -p $G/fonts $G/libs
cp -f /storage/roms/ports/PortMaster/resources/NotoSansSC-Regular.ttf $G/fonts/ 2>/dev/null || \
  cp -f /storage/games-external/ports/PortMaster/resources/NotoSansSC-Regular.ttf $G/fonts/
ls -l $G/fonts
echo "=== binary ==="
ls -l $G/ragingloop
chmod +x $G/ragingloop
echo "=== ldd ==="
ldd $G/ragingloop
echo "=== launcher ==="
ls -l /storage/roms/ports/ragingloop.sh && chmod +x /storage/roms/ports/ragingloop.sh && sed -n '1,12p' /storage/roms/ports/ragingloop.sh
