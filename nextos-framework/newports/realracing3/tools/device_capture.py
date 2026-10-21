#!/usr/bin/env python3
"""Screenshot the running game, by one of two routes, and pull the PNGs back.

    python tools/device_capture.py --host <ip> [--source game|compositor] ...

A running process and a frame counter do not prove video, so this tool answers
the only question that does: what is on the screen. Two routes, because they
answer from different sides and they do not always agree:

  --source game        the port's own control channel
                       ($REALRACING3_CONTROL_DIR/commands, "screenshot TOKEN"),
                       which is a glReadPixels of the default framebuffer done
                       from inside the frame loop, right after the swap. This is
                       what the GPU actually holds.
  --source compositor  `grim`, i.e. what the compositor composites to the panel.
                       Needs no cooperation from the port, but it depends on the
                       driver handing the compositor a buffer it may read: on
                       this console the Mali blob presents by direct scanout and
                       grim then returns an all-black image even though the game
                       is visibly rendering. So a black grim capture is evidence
                       about the capture path, not about the game; corroborate it
                       with --source game before concluding anything.

Default is `game`, because it is the one that works on both drivers. With
--attach the game is already running, which only matters for the game route if
it was started with REALRACING3_CONTROL_DIR set.

The game route needs no rebuild: the launcher starts the run with
REALRACING3_CONTROL_DIR pointed at a scratch directory the port creates itself,
and each shot is one line appended to that directory's `commands` queue.

The --env / timing knobs exist because the debug loop is "change one thing, look
at the screen again" far more often than "rebuild and redeploy".
"""
import argparse, os, re, shlex, sys, time

import paramiko

GAMEDIR = "/storage/roms/ports/realracing3"
SCRIPTS = "/storage/roms/ports/Real Racing 3.sh"
RUNNING = GAMEDIR + "/capture-run.log"
# The launcher redirects its own trace output to this file, so the render-loop
# evidence is here, not in the exec channel's stdout.
LOGFILE = GAMEDIR + "/log.txt"
CONTROL_DIR = GAMEDIR + "/control"


def wait_for(predicate, timeout, period=1.0):
    deadline = time.time() + timeout
    while time.time() < deadline:
        if predicate():
            return True
        time.sleep(period)
    return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default=os.environ.get("DEVICE_HOST"))
    ap.add_argument("--port", type=int, default=22)
    ap.add_argument("--user", default="root")
    ap.add_argument("--password", default=os.environ.get("DEVICE_PASS"))
    ap.add_argument("--env", action="append", default=[], metavar="K=V")
    ap.add_argument("--until", default="onViewRenderJNI #30",
                    help="log substring that means the render loop is live")
    ap.add_argument("--wait", type=int, default=90, help="max seconds for --until")
    ap.add_argument("--settle", type=int, default=0,
                    help="extra seconds of rendering after --until before the shot")
    ap.add_argument("--shots", type=int, default=1, help="screenshots, --gap apart")
    ap.add_argument("--gap", type=int, default=15, help="seconds between shots")
    ap.add_argument("--timeout", type=int, default=40,
                    help="seconds to wait for one shot to appear")
    ap.add_argument("--source", choices=("game", "compositor"), default="game",
                    help="who reads the pixels: the port's own glReadPixels, or grim")
    ap.add_argument("--hold", type=int, default=170, help="seconds before the run is killed")
    ap.add_argument("--attach", action="store_true",
                    help="do not launch anything; just shoot at a run already on screen")
    ap.add_argument("--out", default="logs/capture.png")
    args = ap.parse_args()
    if not args.host:
        raise SystemExit("device host required (--host or DEVICE_HOST)")
    if not args.password:
        raise SystemExit("device password required (--password or DEVICE_PASS); none is stored here")

    exports = {k: v for k, v in (e.split("=", 1) for e in args.env)}
    # The port only opens its command queue when this is set; without it the
    # game route has nothing to write to.
    exports.setdefault("REALRACING3_CONTROL_DIR", CONTROL_DIR)
    export_prefix = " ".join("export %s=%s;" % (k, shlex.quote(v))
                             for k, v in exports.items())
    start = "cd /storage/roms/ports && %s setsid nohup timeout %d bash %s > %s 2>&1 < /dev/null &" % (
        export_prefix, args.hold + 30, shlex.quote(SCRIPTS), RUNNING)

    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(args.host, port=args.port, username=args.user, password=args.password,
                timeout=20, banner_timeout=60, auth_timeout=60)

    def run(cmd):
        _, out, err = ssh.exec_command(cmd, timeout=90)
        o = out.read().decode("utf-8", "replace")
        e = err.read().decode("utf-8", "replace")
        return o, e

    def log_tail():
        text, _ = run("tail -c 200000 %s 2>/dev/null" % LOGFILE)
        return text

    def screenshot_game(token):
        """Ask the running game for its own framebuffer, and fetch the PNG.

        The port reads pixels with glReadPixels of the default framebuffer and
        writes them itself, so the image is what the GPU holds regardless of how
        it reaches the panel. One command per line in <control>/commands; the
        reply appears at <control>/screenshots/<token>.png.
        """
        remote = "%s/screenshots/%s.png" % (CONTROL_DIR, token)
        exists, _ = run("test -e %s && echo open" % shlex.quote(CONTROL_DIR + "/commands"))
        if "open" not in exists:
            return False, ("the run has no command queue at %s/commands: it was "
                           "started without REALRACING3_CONTROL_DIR, so the game "
                           "route cannot reach it (use --attach with a run that "
                           "has it, or --source compositor)" % CONTROL_DIR)
        cmd = ("printf 'screenshot %s\\n' >> %s"
               % (shlex.quote(token), shlex.quote(CONTROL_DIR + "/commands")))
        _, err = run(cmd)
        if err.strip():
            return False, "queue write failed: %s" % err.strip()
        got = wait_for(lambda: "yes" in run("test -e %s && echo yes"
                                            % shlex.quote(remote))[0],
                       args.timeout, 1.0)
        if not got:
            return False, ("no PNG after %d s; the queue was written but nothing "
                           "answered (frame loop stuck, or the screenshot failed "
                           "- see the run log)" % args.timeout)
        return True, remote

    def screenshot(path):
        """Capture the live compositor output.

        grim reaches the compositor over a Wayland socket, so the environment
        it needs is the session's, not ssh's. Both socket styles are tried:
        $XDG_RUNTIME_DIR/wayland-N for a wlroots session, $SWAYSOCK for sway,
        read from the compositor process rather than guessed.
        """
        q = shlex.quote(path)
        wayland = (
            "sock=$(ls -t ${XDG_RUNTIME_DIR:-/var/run/0-runtime-dir}/wayland-* "
            "/run/0-runtime-dir/wayland-* 2>/dev/null | grep -v lock | head -1); "
            "if [ -n \"$sock\" ]; then WAYLAND_DISPLAY=$(basename $sock) "
            "XDG_RUNTIME_DIR=$(dirname $sock) grim %s && echo grim-ok; fi" % q)
        o, e = run(wayland)
        if "grim-ok" in o:
            return True, o.strip()
        sway = (
            "for p in $(pgrep sway 2>/dev/null) $(pgrep -f essway 2>/dev/null); do "
            "env=$(tr '\\0' '\\n' < /proc/$p/environ 2>/dev/null); "
            "s=$(echo \"$env\" | grep '^SWAYSOCK=' | cut -d= -f2-); "
            "r=$(echo \"$env\" | grep '^XDG_RUNTIME_DIR=' | cut -d= -f2-); "
            "if [ -n \"$s\" ]; then SWAYSOCK=\"$s\" XDG_RUNTIME_DIR=\"$r\" grim %s && echo grim-ok && break; fi; "
            "done" % q)
        o2, e2 = run(sway)
        return "grim-ok" in o2, (o + e + " | sway: " + o2 + e2).strip()

    def shoot_all():
        """Take --shots screenshots, --gap seconds apart, and pull them back."""
        base, ext = os.path.splitext(args.out)
        stamp = time.strftime("%H%M%S")
        sftp = ssh.open_sftp()
        for n in range(1, args.shots + 1):
            local = "%s-%d%s" % (base, n, ext)
            if args.source == "game":
                token = "cap%s-%d" % (stamp, n)
                remote = "%s/screenshots/%s.png" % (CONTROL_DIR, token)
                ok, report = screenshot_game(token)
            else:
                remote = "/tmp/cap-%s-%d.png" % (stamp, n)
                ok, report = screenshot(remote)
            print("shot %d (%s): %s" % (n, args.source, report or "no output"))
            if ok:
                try:
                    sftp.get(remote, local)
                    print("captured: %s (%d bytes)" % (local, os.path.getsize(local)))
                except IOError as ex:
                    print("pull failed: %s" % ex)
            if n < args.shots:
                time.sleep(args.gap)
        sftp.close()

    if args.attach:
        shoot_all()
        ssh.close()
        return 0

    run("rm -f %s %s /tmp/cap*.png" % (RUNNING, LOGFILE))
    run("( sleep %d; pkill -f 'loader|realracing3' ) >/dev/null 2>&1 &" % args.hold)
    run(start)
    print("launcher started, waiting for %r" % args.until)

    if not wait_for(lambda: args.until in log_tail(), args.wait):
        print("== did not reach %r within %d s ==" % (args.until, args.wait))
        print(log_tail()[-2000:])
        run("pkill -f 'loader|realracing3' ; true")
        ssh.close()
        return 1
    if args.settle:
        time.sleep(args.settle)

    shoot_all()

    print("== last of run log ==")
    print(log_tail()[-1200:])
    run("pkill -f 'loader|realracing3' ; true")
    ssh.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
