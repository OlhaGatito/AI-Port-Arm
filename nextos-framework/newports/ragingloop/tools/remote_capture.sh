G=/storage/roms/ports/ragingloop
cd $G || exit 1
rm -rf /tmp/rlshots; mkdir -p /tmp/rlshots
export RL_ROOT=$G
export RL_FONT=$G/fonts/NotoSansSC-Regular.ttf
export RL_SNAP_DIR=/tmp/rlshots
export RL_SNAP_EVERY=120
export RL_MAXFRAMES=1200
export RL_AUTOP=25
export SDL_VIDEODRIVER=dummy
echo "=== capture run ==="
./ragingloop > /tmp/rl_run.log 2>&1
echo "rc=$?"
echo "=== log head ==="
head -30 /tmp/rl_run.log
echo "=== miss/todo lines ==="
grep -cE 'MISS' /tmp/rl_run.log
grep -E 'MISS|TODO|failed' /tmp/rl_run.log | sort | uniq -c | sort -rn | head -20
echo "=== tail ==="
tail -12 /tmp/rl_run.log
echo "=== shots ==="
ls -l /tmp/rlshots | head -20
du -sh /tmp/rlshots
