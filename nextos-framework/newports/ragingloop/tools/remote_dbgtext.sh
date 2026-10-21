G=/storage/roms/ports/ragingloop
cd $G || exit 1
export RL_ROOT=$G RL_FONT=$G/fonts/NotoSansSC-Regular.ttf
export RL_MAXFRAMES=400 RL_AUTOP=25 RL_VERBOSE=1
export SDL_VIDEODRIVER=dummy
./ragingloop 2>&1 | grep -vE '^snapshot' | head -60
