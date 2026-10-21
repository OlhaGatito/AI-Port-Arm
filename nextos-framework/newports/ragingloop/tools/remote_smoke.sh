G=/storage/roms/ports/ragingloop
# comm-based match only: an args-based grep also matches this very script and
# would kill the SSH shell running it.
for p in $(ps -eo pid=,comm= | awk '$2=="ragingloop" || $2=="ragingloop.sh" {print $1}'); do
	kill -9 $p 2>/dev/null
done
cd $G || exit 1
rm -f /tmp/rl_snap.bmp /tmp/rl_run.log
export RL_ROOT=$G
export RL_FONT=$G/fonts/NotoSansSC-Regular.ttf
export RL_SNAP=/tmp/rl_snap.bmp
export RL_SNAP_AT=90
export RL_MAXFRAMES=120
export SDL_VIDEODRIVER=kmsdrm
echo "=== run ==="
./ragingloop > /tmp/rl_run.log 2>&1
echo "rc=$?"
echo "=== log ==="
cat /tmp/rl_run.log
echo "=== snap ==="
ls -l /tmp/rl_snap.bmp 2>/dev/null
