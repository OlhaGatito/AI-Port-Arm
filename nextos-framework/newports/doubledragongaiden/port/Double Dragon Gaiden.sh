#!/bin/bash

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

source "$controlfolder/control.txt"

get_controls
[ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"


GAMEDIR="/$directory/ports/doubledragongaiden"

cd "$GAMEDIR"

# logging
> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

mkdir -p "$GAMEDIR/conf"
mkdir -p "$GAMEDIR/cache"
export XDG_DATA_HOME="$GAMEDIR/conf"
export XDG_CONFIG_HOME="$GAMEDIR/conf"


export LD_LIBRARY_PATH="$GAMEDIR/lib:/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# ============================================================
# Generic Display Driver Auto-Detection  (port-agnostic)
# ============================================================
detect_platform() {
  if [ -n "${PLATFORM_OVERRIDE:-}" ]; then
    PLATFORM="$PLATFORM_OVERRIDE"
    return
  fi

  local os_id=""
  if [ -r /etc/os-release ]; then
    os_id=$(. /etc/os-release 2>/dev/null; printf '%s' "${ID:-}")
  fi

  case "$os_id" in
    emuelec|coreelec|libreelec)
      PLATFORM="$os_id" ;;
    *)
      if   grep -qi 'emuelec\|coreelec\|libreelec' /etc/os-release 2>/dev/null \
           || [ -f /etc/emuelec-version ] || [ -d /emuelec ] || [ -d /storage/.config/emuelec ]; then
        PLATFORM=emuelec
      elif [ -n "$WAYLAND_DISPLAY" ] || [ -f /etc/knulli.conf ] || [ -d /userdata/system ]; then
        PLATFORM=wayland
      elif [ -f /etc/muos/config.ini ] || [ -d /opt/muos ] || [ -f /usr/bin/muos ] || [ -d /run/muos ]; then
        PLATFORM=muos
      elif [ -e /dev/dri/card0 ]; then
        PLATFORM=kmsdrm
      else
        PLATFORM=unknown
      fi ;;
  esac
}
detect_platform

case "$PLATFORM" in
  emuelec|coreelec|libreelec)
    unset SDL_VIDEODRIVER
    export SDL_FBDEV=/dev/fb0 SDL_NOMOUSE=1 LIBGL_FB=3 LIBGL_ES=2
    if [ ! -e /dev/fb0 ] && [ -e /dev/dri/card0 ]; then
      export SDL_VIDEODRIVER=kmsdrm
    fi
    if [ -d /usr/lib/libmali ]; then
      export LD_LIBRARY_PATH="/usr/lib/libmali:${LD_LIBRARY_PATH}"
    fi
    ;;
  wayland)
    export SDL_VIDEODRIVER=wayland
    ;;
  muos)
    unset SDL_VIDEODRIVER
    ;;
  kmsdrm)
    [ -z "${SDL_VIDEODRIVER:-}" ] && export SDL_VIDEODRIVER=kmsdrm
    if [ -d /usr/lib/libmali ]; then
      export LD_LIBRARY_PATH="/usr/lib/libmali:${LD_LIBRARY_PATH}"
    fi
    ;;
  *) : ;;
esac

echo "PLATFORM=$PLATFORM SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-unset}"
echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH"

export SDL_GAMECONTROLLERCONFIG_FILE="${GAMEDIR}/gamecontrollerdb.txt"
export SDL_GAMECONTROLLER_USE_BUTTON_LABELS=0
export MALLOC_ARENA_MAX=${MALLOC_ARENA_MAX:-2}

export SF_LOGCAT=${SF_LOGCAT:-1}
export SF_FRAMES=${SF_FRAMES:-0}

if pgrep -x pulseaudio >/dev/null 2>&1 || pgrep -x pipewire-pulse >/dev/null 2>&1; then
  export SDL_AUDIODRIVER=pulse
else
  export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-alsa}"
  if [ -z "${AUDIODEV:-}" ] && grep -qi audiocodec /proc/asound/cards 2>/dev/null; then
    export AUDIODEV=plughw:audiocodec
  fi
fi

chmod a+x "$GAMEDIR/ddgaiden-nextos"
pm_platform_helper "$GAMEDIR/ddgaiden-nextos"


"$GAMEDIR/ddgaiden-nextos" "$GAMEDIR"

pm_finish
