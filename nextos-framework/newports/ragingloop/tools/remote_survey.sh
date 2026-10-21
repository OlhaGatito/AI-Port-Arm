G=/storage/roms/ports/ragingloop
cd $G || exit 1
ps -eo pid=,comm= | awk '$2=="ragingloop"{print $1}' | xargs -r kill -9 2>/dev/null
sleep 1
mv $G/ragingloop.new $G/ragingloop
chmod +x $G/ragingloop
echo "== swapped =="
sha256sum $G/ragingloop
echo "== loader =="
ldd $G/ragingloop 2>&1 | grep -Ei 'vorbis|ogg|SDL2|not found'
rm -rf /tmp/rlsurv; mkdir -p /tmp/rlsurv
rm -f /tmp/rlsurv.log
export RL_ROOT=$G RL_FONT=$G/fonts/NotoSansSC-Regular.ttf
export RL_SNAP_DIR=/tmp/rlsurv RL_SNAP_EVERY=150 RL_MAXFRAMES=900 RL_AUTOP=18
export RL_VERBOSE=1 RL_SCRIPT=${RLS:-scenario02_victim} SDL_VIDEODRIVER=dummy
setsid ./ragingloop > /tmp/rlsurv.log 2>&1 < /dev/null &
echo "launched pid=$!"
