#!/bin/sh
# Device-side launcher for the Broforce port.
#   /tmp/bfrun.sh <logfile>
# Extra environment is read from /tmp/bfenv.sh (written separately).
GAMEDIR=/roms/ports/broforce
LOG=${1:-/tmp/bfrun.log}

systemctl stop emustation 2>/dev/null
killall -9 emulationstation 2>/dev/null
sleep 2

cd "$GAMEDIR" || exit 1
export SDL_VIDEODRIVER=kmsdrm
export SDL_NOMOUSE=1
export XDG_DATA_HOME="$GAMEDIR/conf"
export XDG_CONFIG_HOME="$GAMEDIR/conf"
export LD_LIBRARY_PATH="$GAMEDIR/lib:/usr/lib"
export SF_LOGCAT=1
export MALLOC_ARENA_MAX=2
[ -r /tmp/bfenv.sh ] && . /tmp/bfenv.sh

rm -f "$LOG"
nohup "$GAMEDIR/broforce-nextos" "$GAMEDIR" > "$LOG" 2>&1 &
echo "started pid=$! log=$LOG"
