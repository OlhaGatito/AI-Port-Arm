G=/storage/roms/ports/ragingloop
cd $G || exit 1
rm -rf /tmp/rlshots; mkdir -p /tmp/rlshots
export RL_ROOT=$G
export RL_FONT=$G/fonts/NotoSansSC-Regular.ttf
export RL_SNAP_DIR=/tmp/rlshots
export RL_SNAP_EVERY=100000
export RL_MAXFRAMES=1200
export RL_AUTOP=25
export SDL_VIDEODRIVER=dummy
gdb -batch -ex run -ex bt -ex 'info registers' --args ./ragingloop 2>&1 | tail -45
