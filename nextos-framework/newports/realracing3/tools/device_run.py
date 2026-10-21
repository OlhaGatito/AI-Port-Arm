#!/usr/bin/env python3
"""Run the deployed RR3 launcher on the device and pull back its log.

    python tools/device_run.py --host <ip> [--env K=V ...] [--timeout 120]

Separate from deploy_port.py because the debug loop is "change an env var, run
again" far more often than "rebuild, upload, run": re-uploading an 11 MB loader
every time is the slow part, not the run.

The launcher redirects its own output to <gamedir>/log.txt, so the log is read
back over SFTP rather than from the exec channel's stdout.
"""
import argparse, os, shlex, sys, time

import paramiko

GAMEDIR = "/storage/roms/ports/realracing3"
SCRIPTS = "/storage/roms/ports/Real Racing 3.sh"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default=os.environ.get("DEVICE_HOST"))
    ap.add_argument("--port", type=int, default=22)
    ap.add_argument("--user", default="root")
    ap.add_argument("--password", default=os.environ.get("DEVICE_PASS"))
    ap.add_argument("--env", action="append", default=[], metavar="K=V",
                    help="exported before the launcher (repeatable)")
    ap.add_argument("--timeout", type=int, default=120, help="hard cap on the run (s)")
    ap.add_argument("--out", help="local file to write the device log to")
    args = ap.parse_args()
    if not args.host:
        raise SystemExit("device host required (--host or DEVICE_HOST)")
    if not args.password:
        raise SystemExit("device password required (--password or DEVICE_PASS); none is stored here")

    exports = " ".join("export %s=%s;" % (k, shlex.quote(v))
                       for k, v in (e.split("=", 1) for e in args.env))
    cmd = ("cd /storage/roms/ports && %s timeout %d bash %s; echo \"run_exit=$?\""
           % (exports, args.timeout, shlex.quote(SCRIPTS)))

    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(args.host, port=args.port, username=args.user,
                password=args.password, timeout=20, banner_timeout=60, auth_timeout=60)
    start = time.time()
    _, out, err = ssh.exec_command(cmd, timeout=args.timeout + 120)
    stdout = out.read().decode("utf-8", "replace")
    stderr = err.read().decode("utf-8", "replace")
    print("=== launcher stdout (%.0f s) ===" % (time.time() - start))
    print(stdout[-3000:])
    if stderr.strip():
        print("=== stderr ===\n" + stderr[-1500:])

    sftp = ssh.open_sftp()
    with sftp.open(GAMEDIR + "/log.txt", "rb") as f:
        log = f.read()
    sftp.close()
    ssh.close()

    text = log.decode("utf-8", "replace")
    if args.out:
        with open(args.out, "wb") as f:
            f.write(log)
        print("=== log written: %s (%d bytes) ===" % (args.out, len(log)))
    print("=== log.txt ===")
    print(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
