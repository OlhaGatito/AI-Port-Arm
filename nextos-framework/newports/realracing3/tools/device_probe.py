#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Read-only device probe for the RR3 ARMv7 target (handheld Linux CFW images).

Host is never hardcoded: --host or DEVICE_HOST. Neither is the password: it
comes from -p or DEVICE_PASS and is not stored in this repository, only the
convention of which *user* each CFW logs in as. Everything executed here is a
read-only inspection command; nothing is created, modified or deleted remotely.
Output is printed and written to a gitignored log under newports/realracing3/logs.
"""
import argparse
import datetime
import json
import os
import sys

try:
    import paramiko
except ImportError:
    sys.exit("[FAIL] paramiko not installed")

USERS = {
    "rocknix": "root",
    "ee47": "root",
    "dArkos": "ark",
}

# (section, command)
CHECKS = [
    ("identity", "uname -a; uname -m; getconf LONG_BIT"),
    ("os-release", "cat /etc/os-release 2>/dev/null | head -12"),
    ("cpu", "head -30 /proc/cpuinfo"),
    ("memory", "grep -E 'MemTotal|MemAvailable' /proc/meminfo"),
    ("drm-panel", "for f in /sys/class/drm/card*-*/modes; do echo \"== $f\"; head -6 \"$f\"; done 2>/dev/null"),
    ("drm-cards", "ls -l /dev/dri 2>/dev/null; ls /sys/class/drm 2>/dev/null"),
    ("gpu-userspace", "ls -l /usr/lib/libmali* /usr/lib/*/libmali* 2>/dev/null; ldconfig -p 2>/dev/null | grep -iE 'libmali|libEGL|libGLESv1|libGLESv2|vulkan' | head -30"),
    ("sdl", "ldconfig -p 2>/dev/null | grep -iE 'libSDL2|libSDL3|libSDL2_image|libSDL2_ttf' | head -20"),
    ("other-libs", "ldconfig -p 2>/dev/null | grep -iE 'libfmod|libopenal|libvorbis|libogg|libarchive|libpng|libz\\.so' | head -30"),
    ("glibc", "ldd --version 2>/dev/null | head -2"),
    ("compat-32bit", "for p in /lib/ld-linux-armhf.so.3 /usr/lib/ld-linux-armhf.so.3 /lib/ld-linux-aarch64.so.1; do [ -e \"$p\" ] && echo \"EXISTS $p\" || echo \"MISSING $p\"; done; ls -d /usr/arm-linux-gnueabihf /usr/lib/arm-linux-gnueabihf /opt/32bit 2>/dev/null"),
    ("kernel-compat", "grep -E 'CONFIG_COMPAT|CONFIG_ARM_AMBA|CONFIG_CPU_V7' /proc/config.gz 2>/dev/null || zcat /proc/config.gz 2>/dev/null | grep -E 'CONFIG_COMPAT=|CONFIG_ARCH_MULTI' | head -5 || echo 'no /proc/config.gz'"),
    ("toolchain", "for t in readelf objdump gcc aarch64-linux-gnu-gcc arm-linux-gnueabihf-gcc cmake git python3; do command -v $t >/dev/null 2>&1 && echo \"have $t\" || echo \"no  $t\"; done"),
    ("ports-dir", "pwd; ls -ld /storage /storage/roms /storage/roms/ports 2>/dev/null; ls -ld $HOME/roms/ports 2>/dev/null"),
    ("ports-listing", "ls -l /storage/roms/ports 2>/dev/null | head -60"),
    ("ports-sh", "ls -l /storage/roms/ports/*.sh 2>/dev/null; echo '--- scripts dir ---'; ls -ld /storage/roms/ports_scripts 2>/dev/null && ls -l /storage/roms/ports_scripts/*.sh 2>/dev/null | head -20"),
    ("rr3-here", "ls -d /storage/roms/ports/*[Rr]eal* /storage/roms/ports/*[Rr]acing* /storage/roms/ports/*[Rr][Rr]3* 2>/dev/null || echo 'no RR3 directory on device'"),
    ("portmaster", "ls -ld /storage/roms/ports/.portmaster 2>/dev/null; find /storage/roms -maxdepth 2 -iname 'portmaster*' 2>/dev/null | head -10; ls /storage/.config/ 2>/dev/null | head -20"),
    ("disk", "df -h /storage / 2>/dev/null | head -6"),
    ("sdl-env", "env | grep -iE 'SDL|DISPLAY|XDG|WAYLAND' | head -20; echo '--- running ---'; ps aux 2>/dev/null | grep -iE 'emulationstation|es-frontend|weston|kmsgrab' | grep -v grep | head -8"),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-H", "--host")
    ap.add_argument("-P", "--port", type=int, default=22)
    ap.add_argument("-u", "--user")
    ap.add_argument("-p", "--password")
    ap.add_argument("-s", "--system", default="rocknix", choices=sorted(USERS),
                    help="selects the login *user* convention only; the password is never stored")
    ap.add_argument("--cmd", action="append", default=[], help="extra read-only command (label is the command itself)")
    ap.add_argument("--skip-base", action="store_true", help="run only the --cmd list")
    ap.add_argument("--cmd-file", help="file with one read-only command per line (blank lines ignored)")
    ap.add_argument("--decode", default="utf-8", help="codec for device output (use gb18030 for GBK-named files on exfat)")
    ap.add_argument("-o", "--out", help="log file path (default logs/device-probe-<ts>.txt)")
    args = ap.parse_args()
    if args.cmd_file:
        with open(args.cmd_file, encoding="utf-8") as fh:
            args.cmd = args.cmd + [ln.strip() for ln in fh if ln.strip() and not ln.startswith("#")]

    host = args.host or os.environ.get("DEVICE_HOST")
    if not host:
        sys.exit("[FAIL] device host required (--host or DEVICE_HOST)")
    user = args.user or os.environ.get("DEVICE_USER") or USERS[args.system]
    password = args.password or os.environ.get("DEVICE_PASS")
    if not password:
        sys.exit("[FAIL] device password required (-p or DEVICE_PASS); none is stored here")

    c = paramiko.SSHClient()
    c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    t0 = datetime.datetime.now()
    try:
        c.connect(host, port=args.port, username=user, password=password,
                  timeout=20, look_for_keys=False, allow_agent=False)
    except Exception as e:
        sys.exit(f"[FAIL] connect {user}@{host}:{args.port}: {type(e).__name__}: {e}")

    lines = [f"# device probe {user}@{host}  {t0.isoformat(timespec='seconds')}", ""]
    base = [] if args.skip_base else CHECKS
    for label, cmd in base + [(cmd, cmd) for cmd in args.cmd]:
        lines.append(f"===== {label} =====")
        try:
            _, o, e = c.exec_command(cmd, timeout=45)
            out = o.read().decode(args.decode, "replace").strip()
            err = e.read().decode(args.decode, "replace").strip()
            lines.append(out if out else "(no output)")
            if err:
                lines.append(f"[stderr] {err[:400]}")
        except Exception as ex:
            lines.append(f"[error] {type(ex).__name__}: {ex}")
        lines.append("")
    c.close()

    text = "\n".join(lines)
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
    print(text)
    out = args.out
    if not out:
        out_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "logs")
        os.makedirs(out_dir, exist_ok=True)
        out = os.path.join(out_dir, f"device-probe-{t0.strftime('%Y%m%d-%H%M%S')}.txt")
    with open(out, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(text)
    print(f"[log] {out}")


if __name__ == "__main__":
    main()
