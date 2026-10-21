#!/usr/bin/env python3
"""Push the built loader + launcher onto the device and run it once, capturing logs.

    python tools/deploy_port.py --host <ip> [--run] [--timeout 120]

Uploads are whole-file (the binary is one file), and every text file is written
with LF endings — a CRLF shebang fails on the device with `bad interpreter`,
which looks like a port bug and is not one. `--run` executes the launcher and
pulls back its log, because the launcher redirects its own output to log.txt.
"""
import argparse, os, posixpath, re, shlex, sys, time

import paramiko

GAMEDIR = "/storage/roms/ports/realracing3"
SCRIPTS = "/storage/roms/ports/Real Racing 3.sh"


def connect(host, port, user, password):
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(host, port=port, username=user, password=password, timeout=20,
                banner_timeout=40, auth_timeout=40)
    return ssh


def put(ssh, local, remote, text=False, mode=None):
    # mkdir -p over the shell rather than SFTP: paramiko 5 dropped
    # SFTPClient.makedirs, and every device in scope has a shell.
    run(ssh, "mkdir -p %s" % shlex.quote(posixpath.dirname(remote)))
    sftp = ssh.open_sftp()
    if text:
        data = open(local, "rb").read().replace(b"\r\n", b"\n")
        with sftp.open(remote, "wb") as f:
            f.write(data)
    else:
        sftp.put(local, remote)
    if mode is not None:
        sftp.chmod(remote, mode)
    sftp.close()
    return os.path.getsize(local)


def run(ssh, cmd, timeout=600):
    _, out, err = ssh.exec_command(cmd, timeout=timeout)
    data = out.read().decode("utf-8", "replace")
    errs = err.read().decode("utf-8", "replace")
    return out.channel.recv_exit_status(), data, errs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default=os.environ.get("DEVICE_HOST"))
    ap.add_argument("--port", type=int, default=22)
    ap.add_argument("--user", default="root")
    ap.add_argument("--password", default=os.environ.get("DEVICE_PASS"))
    ap.add_argument("--build", default="port/build", help="local build dir")
    ap.add_argument("--launcher", default="port/package/Real Racing 3.sh")
    ap.add_argument("--no-libs", action="store_true")
    # The launcher puts libs.armhf at the FRONT of LD_LIBRARY_PATH, so a bundled
    # library also shadows a newer one the device already provides. On a device
    # that has libstdc++ 6.0.33 and zlib 1.3.1, shipping the build image's older
    # copies is a downgrade, not a fallback. Send only what is actually missing.
    ap.add_argument("--only-lib", action="append", default=[],
                    metavar="SONAME", help="upload only this libs.armhf entry (repeatable)")
    ap.add_argument("--run", action="store_true")
    ap.add_argument("--timeout", type=int, default=120)
    ap.add_argument("--tail", type=int, default=80, help="log lines to print")
    args = ap.parse_args()
    if not args.host:
        raise SystemExit("device host required (--host or DEVICE_HOST)")
    if not args.password:
        raise SystemExit("device password required (--password or DEVICE_PASS); none is stored here")

    binary = os.path.join(args.build, "realracing3")
    if not os.path.isfile(binary):
        raise SystemExit("no %s — build first" % binary)

    ssh = connect(args.host, args.port, args.user, args.password)
    print("upload %s (%d bytes)" % (binary, put(ssh, binary, GAMEDIR + "/realracing3", mode=0o755)))
    put(ssh, args.launcher, SCRIPTS, text=True, mode=0o755)
    print("upload launcher -> %s (LF enforced)" % SCRIPTS)

    libs = os.path.join(args.build, "libs.armhf")
    if not args.no_libs and os.path.isdir(libs):
        names = sorted(os.listdir(libs))
        if args.only_lib:
            names = [n for n in names if n in args.only_lib]
        for name in names:
            src = os.path.join(libs, name)
            if os.path.isfile(src):
                put(ssh, src, GAMEDIR + "/libs.armhf/" + name)
        print("upload libs.armhf: %d files (%s)"
              % (len(names), " ".join(names) if names else "none"))
    elif not args.no_libs:
        print("no %s — device SDL/glibc will be used as-is" % libs)

    status, out, errs = run(ssh, "ls -la %s; echo; head -3 %s"
                            % (shlex.quote(GAMEDIR), shlex.quote(SCRIPTS)))
    print("remote listing (exit %d):\n%s%s" % (status, out, errs))

    if args.run:
        print("=== running launcher, %d s cap ===" % args.timeout)
        start = time.time()
        cmd = "cd /storage/roms/ports && timeout %d bash %s" % (args.timeout, shlex.quote(SCRIPTS))
        status, out, errs = run(ssh, cmd, timeout=args.timeout + 90)
        print("launcher exit=%d after %.0f s" % (status, time.time() - start))
        if out.strip():
            print(out[-4000:])
        if errs.strip():
            print("STDERR:\n" + errs[-2000:])
        status, out, errs = run(ssh, "tail -c 200000 '%s/log.txt' 2>/dev/null" % GAMEDIR)
        lines = out.splitlines()
        print("=== log.txt (%d lines shown of %d) ===" % (min(args.tail, len(lines)), len(lines)))
        for line in lines[-args.tail:]:
            print(line)
    ssh.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
