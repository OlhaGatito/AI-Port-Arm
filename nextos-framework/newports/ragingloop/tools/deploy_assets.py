#!/usr/bin/env python3
"""Stream the ragingloop game payload onto the test handheld as one tar pipe.

Why a tar stream instead of per-file SFTP: the payload is ~7600 small voice files
plus ~470 images; per-file SFTP round-trips dominate. One `tar -x` on the device
takes the whole stream.

Layout produced on the device (merged PortMaster layout, everything under one
port directory):

    <remote>/assets/...              from apk/assets (minus RestoreToGame, SDK blobs)
    <remote>/files/voice|sound       the payload the Java port unpacked at first run
    <remote>/save/shared_prefs       the shipped prefs seed (progress/setting keys)

Incremental: a device-side size manifest is fetched first and files whose size
matches are skipped. Re-run after an interruption.

  DEVICE_HOST=<ip> python tools/deploy_assets.py \
      --apk apk --remote /storage/roms/ports/ragingloop --system rocknix
"""
import argparse
import os
import posixpath
import subprocess
import sys
import tarfile
import time

import paramiko

CREDS = {"ee47": ("root", "emuelec"), "rocknix": ("root", "rocknix")}
EXCLUDE_TOP = {"RestoreToGame", "eagle_simple_sdks_archives"}


def dev_manifest(ssh, remote):
    """{posix path: size} for files already on the device."""
    cmd = ("cd %s 2>/dev/null && find . -type f -exec stat -c '%%s %%n' {} + "
           "|| true" % remote)
    _, out, _ = ssh.exec_command(cmd, timeout=600)
    table = {}
    for line in out.read().decode("utf-8", "replace").splitlines():
        parts = line.split(" ", 1)
        if len(parts) == 2 and parts[0].isdigit():
            table[parts[1].lstrip("./")] = int(parts[0])
    out.channel.recv_exit_status()
    return table


def plan(apk):
    """[(abs_src, arcname)] honouring the layout mapping."""
    items = []
    aroot = os.path.join(apk, "assets")
    for dp, dn, fn in os.walk(aroot):
        rel = os.path.relpath(dp, aroot).replace(os.sep, "/")
        top = rel.split("/")[0]
        if rel != "." and top in EXCLUDE_TOP:
            dn[:] = []
            continue
        for f in sorted(fn):
            if f == ".DS_Store" or f.endswith(".apk"):
                continue
            p = os.path.join(dp, f)
            items.append((p, "assets/" + (rel + "/" if rel != "." else "") + f))
    files = os.path.join(aroot, "RestoreToGame", "external_data", "files")
    for sub in ("voice", "sound"):
        d = os.path.join(files, sub)
        if os.path.isdir(d):
            for dp, dn, fn in os.walk(d):
                for f in sorted(fn):
                    p = os.path.join(dp, f)
                    arc = "files/" + os.path.relpath(p, files).replace(os.sep, "/")
                    items.append((p, arc))
    prefs = os.path.join(aroot, "RestoreToGame", "data", "shared_prefs")
    if os.path.isdir(prefs):
        for f in sorted(os.listdir(prefs)):
            if f.endswith(".xml"):
                items.append((os.path.join(prefs, f), "save/shared_prefs/" + f))
    return items


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--apk", default="apk")
    ap.add_argument("--remote", required=True)
    ap.add_argument("--system", default="rocknix")
    ap.add_argument("--host", default=os.environ.get("DEVICE_HOST"))
    ap.add_argument("--force", action="store_true")
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    if not a.host:
        sys.exit("set --host or DEVICE_HOST (address must come from this task)")
    if not a.remote.startswith("/"):
        # Git-Bash rewrites "/storage/x" into "D:/Programs/Git/storage/x"; the
        # device then treats it as relative and builds a junk tree under $HOME.
        sys.exit("remote path must be absolute; Git-Bash mangled it to %r "
                 "(export MSYS_NO_PATHCONV=1)" % a.remote)

    items = plan(a.apk)
    total = sum(os.path.getsize(p) for p, _ in items if os.path.exists(p))
    print("planned files=%d bytes=%.1f MB" % (len(items), total / 1048576.0))
    if a.dry_run:
        for p, arc in items[:20]:
            print("   %s -> %s" % (arc, os.path.getsize(p)))
        return

    user, pw = CREDS[a.system]
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(a.host, username=user, password=pw, timeout=20)
    _, o, _ = ssh.exec_command("mkdir -p '%s'" % a.remote)
    o.channel.recv_exit_status()
    have = {} if a.force else dev_manifest(ssh, a.remote)
    todo = [(p, arc) for p, arc in items
            if arc not in have or have[arc] != os.path.getsize(p)]
    print("device has %d entries; to send %d" % (len(have), len(todo)))
    if not todo:
        print("nothing to do")
        return

    missing = [arc for p, arc in todo if not os.path.exists(p)]
    if missing:
        print("WARN %d planned files absent locally, e.g. %s" %
              (len(missing), missing[:3]))
    todo = [(p, arc) for p, arc in todo if os.path.exists(p)]
    nbytes = sum(os.path.getsize(p) for p, _ in todo)

    chan = ssh.get_transport().open_session()
    chan.settimeout(None)
    errlog = "/tmp/rl_deploy.tar.err"
    chan.exec_command("tar -x -f - -C '%s' 2> %s" % (a.remote, errlog))
    sent = [0]
    t0 = time.time()

    class Sink:
        def write(self, data):
            chan.sendall(data)
            sent[0] += len(data)
            pct = 100.0 * sent[0] / nbytes
            if int(sent[0] // (32 << 20)) != int((sent[0] - len(data)) // (32 << 20)):
                el = time.time() - t0
                print("  %5.1f%%  %.0f/%.0f MB  %.1f MB/s  eta %.0fs"
                      % (pct, sent[0] / 1048576.0, nbytes / 1048576.0,
                         sent[0] / 1048576.0 / max(el, 0.01),
                         (nbytes - sent[0]) / max(sent[0] / el / 1048576.0, 0.01)),
                      flush=True)
        def close(self):
            pass

    with tarfile.open(fileobj=Sink(), mode="w|") as tf:
        for p, arc in todo:
            tf.add(p, arcname=arc, recursive=False)
    chan.shutdown_write()
    rc = chan.recv_exit_status()
    _, oerr, _ = ssh.exec_command("cat %s; rm -f %s" % (errlog, errlog), timeout=60)
    terr = oerr.read().decode("utf-8", "replace").strip()
    if terr:
        print("tar stderr: %s" % terr)
    el = time.time() - t0
    print("tar rc=%d sent %.1f MB in %.1fs (%.1f MB/s)"
          % (rc, sent[0] / 1048576.0, el, sent[0] / 1048576.0 / max(el, 0.01)))
    verify = ("cd '%s' && find . -type f | wc -l && du -sh . "
              "&& ls assets | head -8 && ls files 2>/dev/null | head -4" % a.remote)
    _, out, _ = ssh.exec_command(verify, timeout=300)
    print(out.read().decode("utf-8", "replace"))
    ssh.close()


main()
