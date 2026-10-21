#!/bin/bash
# Beastie Bay for NextOS -- ArkOS launcher (single-directory layout).
# Structural base: ports/fp2-nextos (same engine generation, GPL-3.0-only).

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}
controlfolder="/roms/ports/PortMaster"
[ -d "/opt/system/Tools/PortMaster" ] && controlfolder="/opt/system/Tools/PortMaster"
[ -f "$controlfolder/control.txt" ] && source "$controlfolder/control.txt"
get_controls 2>/dev/null

GAMEDIR="/roms/ports/beastiebay"
cd "$GAMEDIR" || exit 1

# logging
> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

mkdir -p "$GAMEDIR/conf" "$GAMEDIR/cache"
export XDG_DATA_HOME="$GAMEDIR/conf"
export XDG_CONFIG_HOME="$GAMEDIR/conf"

# FP2-lineage env contract: ArkOS EGL driver names, nxinput receipts.
. "$GAMEDIR/port-env.sh"

export SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-kmsdrm}
export LD_LIBRARY_PATH="$GAMEDIR/lib:/usr/local/lib/aarch64-linux-gnu:/usr/lib/aarch64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export MALLOC_ARENA_MAX=${MALLOC_ARENA_MAX:-2}

export FP2_NONNULL_OBJECT_FALLBACK=${FP2_NONNULL_OBJECT_FALLBACK:-1}
export FP2_MISSLOG=${FP2_MISSLOG:-1}
export FP2_LOGCAT=${FP2_LOGCAT:-1}
export FP2_JNILOG=${FP2_JNILOG:-1}
export FP2_VERBOSE=${FP2_VERBOSE:-1}
export FP2_GLLOG=${FP2_GLLOG:-0}
export FP2_SCREENSHOT=${FP2_SCREENSHOT:-/roms/ports/beastiebay/shot.ppm}
export FP2_SCREENSHOT_FRAME=${FP2_SCREENSHOT_FRAME:-1200}
export FP2_FRAMES=${FP2_FRAMES:-0}

echo "PLATFORM=arkos SDL_VIDEODRIVER=$SDL_VIDEODRIVER"
echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH"

chmod a+x "$GAMEDIR/beastiebay-nextos"
"$GAMEDIR/beastiebay-nextos" "$GAMEDIR"

pm_finish 2>/dev/null || true
