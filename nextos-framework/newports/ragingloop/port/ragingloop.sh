#!/bin/bash
# Raging Loop (人狼村之谜) — KEMCO Java ADV engine re-implemented as a native
# AArch64 interpreter. Official PortMaster launcher template, merged layout:
# this file lives in roms/ports/, the payload lives in roms/ports/ragingloop/.

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}

if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi

source $controlfolder/control.txt
source $controlfolder/device_info.txt

[ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"

get_controls

GAMEDIR="/$directory/ports/ragingloop"

> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

cd "$GAMEDIR"
$ESUDO chmod -R +x .

# The loader needs only the firmware's own SDL2 (verified with ldd on device),
# so libs/ is reserved for assets the port is missing, never for system clones.
export LD_LIBRARY_PATH="$GAMEDIR/libs:${LD_LIBRARY_PATH}"
export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"

# No gptokeyb: SDL2 opens the evdev gamepad directly through the config above,
# and a key-injecting wrapper would double every button press.

# Video driver is left to the frontend: this device runs sway, which exports
# SDL_VIDEODRIVER=wayland + WAYLAND_DISPLAY, and the on-device log confirms the
# window came up as "sdl 2.32.10 driver=wayland / renderer=opengles2" at 30 fps.
# Forcing a fallback here only breaks that proven path.

export RL_ROOT="$GAMEDIR"
if [ -f "$GAMEDIR/fonts/NotoSansSC-Regular.ttf" ]; then
  export RL_FONT="$GAMEDIR/fonts/NotoSansSC-Regular.ttf"
elif [ -f "$controlfolder/resources/NotoSansSC-Regular.ttf" ]; then
  export RL_FONT="$controlfolder/resources/NotoSansSC-Regular.ttf"
fi

pm_platform_helper "./ragingloop"
RL_VERBOSE=1 ./ragingloop
pm_finish
