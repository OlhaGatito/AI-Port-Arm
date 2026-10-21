G=/storage/roms/ports/ragingloop
cd $G || exit 1
export RL_ROOT=$G
export RL_FONT=$G/fonts/NotoSansSC-Regular.ttf
export RL_MAXFRAMES=10
export RL_SNAP=/tmp/rl_snap.bmp
export RL_SNAP_AT=5
echo "=== A: kmsdrm, unbuffered live ==="
SDL_VIDEODRIVER=kmsdrm ./ragingloop 2>&1 | tail -25
echo "rcA=${PIPESTATUS[0]}"
echo "=== B: dummy driver ==="
SDL_VIDEODRIVER=dummy ./ragingloop 2>&1 | tail -25
echo "rcB=${PIPESTATUS[0]}"
echo "=== C: nothing else on screen? ==="
ls -l /tmp/rl_snap.bmp 2>/dev/null
dmesg | tail -3
