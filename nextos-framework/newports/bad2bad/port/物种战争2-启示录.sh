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


GAMEDIR="/$directory/ports/bad2bad"

cd "$GAMEDIR"

# logging
> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

mkdir -p "$GAMEDIR/conf"
mkdir -p "$GAMEDIR/cache"
export XDG_DATA_HOME="$GAMEDIR/conf"
export XDG_CONFIG_HOME="$GAMEDIR/conf"


# Game .so first, then system libs. Never bundle Debian SDL.
export LD_LIBRARY_PATH="$GAMEDIR/lib:/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# ============================================================
# Generic Display Driver Auto-Detection  (port-agnostic)
# 不绑定任何具体游戏/平台名；仅用单一 PLATFORM 变量驱动分发。
# ============================================================

# ---- 1) 探测平台 → PLATFORM（单一 token） ----
detect_platform() {
  # 通用覆盖钩子：任意端口均可用 PLATFORM_OVERRIDE 强制指定平台
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
      # 回退探测：按显示栈特征识别，而非硬编码游戏名
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

# ---- 2) 按 PLATFORM 分发 SDL 环境变量（单一真相来源） ----
case "$PLATFORM" in
  emuelec|coreelec|libreelec)
    unset SDL_VIDEODRIVER
    export SDL_FBDEV=/dev/fb0 SDL_NOMOUSE=1 LIBGL_FB=3 LIBGL_ES=2
    # EE overlay: pure-DRM images may have no fb0; fall back to kmsdrm
    if [ ! -e /dev/fb0 ] && [ -e /dev/dri/card0 ]; then
      export SDL_VIDEODRIVER=kmsdrm
    fi
    # Mali CFW: glvnd vendor dir is empty; libmali must win over Mesa in /usr/lib
    if [ -d /usr/lib/libmali ]; then
      export LD_LIBRARY_PATH="/usr/lib/libmali:${LD_LIBRARY_PATH}"
    fi
    ;;
  wayland)
    export SDL_VIDEODRIVER=wayland
    ;;
  muos)
    unset SDL_VIDEODRIVER   # 交给 SDL 自动探测显示
    ;;
  kmsdrm)
    [ -z "${SDL_VIDEODRIVER:-}" ] && export SDL_VIDEODRIVER=kmsdrm
    if [ -d /usr/lib/libmali ]; then
      export LD_LIBRARY_PATH="/usr/lib/libmali:${LD_LIBRARY_PATH}"
    fi
    ;;
  *) : ;;  # 未识别：保留 SDL 默认
esac

echo "PLATFORM=$PLATFORM SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-unset} WAYLAND_DISPLAY=${WAYLAND_DISPLAY:-unset}"
echo "SDL_FBDEV=${SDL_FBDEV:-unset} LIBGL_FB=${LIBGL_FB:-unset} LIBGL_ES=${LIBGL_ES:-unset}"
echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH"

export SDL_GAMECONTROLLERCONFIG_FILE="${GAMEDIR}/gamecontrollerdb.txt"
export SDL_GAMECONTROLLER_USE_BUTTON_LABELS=0
export MALLOC_ARENA_MAX=${MALLOC_ARENA_MAX:-2}

# NextOS loader knobs
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

chmod a+x "$GAMEDIR/bad2bad-nextos"
pm_platform_helper "$GAMEDIR/bad2bad-nextos"


"$GAMEDIR/bad2bad-nextos" "$GAMEDIR"

pm_finish
