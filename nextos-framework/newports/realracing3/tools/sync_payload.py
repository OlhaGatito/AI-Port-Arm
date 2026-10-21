#!/usr/bin/env python3
"""Stream the owner's game tree onto the device, incrementally.

The payload is ~11k files; per-file SFTP over the link takes longer than the
transfer itself, so this packs the missing files into tar streams and pipes them
straight into `tar xf -` on the device. Resume is by path+size, which is enough
for a one-way copy of an immutable tree.

    python tools/sync_payload.py --host <ip> --local apk --remote /storage/roms/ports/realracing3

The host is only ever taken from the command line or DEVICE_HOST; it is never
read from a file, so an old address in the docs cannot be used by accident.
"""
import argparse, hashlib, io, os, posixpath, re, sys, tarfile, time

import paramiko

CHUNK_BYTES = 400 * 1024 * 1024

REMOTE_MANIFEST = r"""
import os, sys
root = sys.argv[1]
for dirpath, dirnames, filenames in os.walk(root):
    for name in filenames:
        p = os.path.join(dirpath, name)
        try:
            st = os.lstat(p)
        except OSError:
            continue
        if os.path.islink(p) or not os.path.isfile(p):
            continue
        sys.stdout.write("%d\t%s\n" % (st.st_size, os.path.relpath(p, root)))
"""

REMOTE_HASH = r"""
import hashlib, sys
h = hashlib.sha256()
with open(sys.argv[1], "rb") as f:
    for block in iter(lambda: f.read(1 << 20), b""):
        h.update(block)
print(h.hexdigest())
"""


class ChannelFile(io.RawIOBase):
    def __init__(self, channel):
        self.channel = channel
        self.closed_flag = False

    def writable(self):
        return True

    def write(self, data):
        if not data:
            return 0
        view = memoryview(data)
        while view:
            n = self.channel.send(view)
            if n <= 0:
                raise IOError("device closed the tar pipe")
            view = view[n:]
        return len(data)


def connect(host, port, user, password):
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(host, port=port, username=user, password=password, timeout=20,
                banner_timeout=40, auth_timeout=40)
    return ssh


def run(ssh, cmd):
    _, out, err = ssh.exec_command(cmd, timeout=1800)
    data = out.read().decode("utf-8", "replace")
    errs = err.read().decode("utf-8", "replace")
    status = out.channel.recv_exit_status()
    return status, data, errs


def push_helper(ssh, name, code):
    sftp = ssh.open_sftp()
    try:
        with sftp.open("/tmp/" + name, "w") as f:
            f.write(code)
    finally:
        sftp.close()
    return "/tmp/" + name


def remote_manifest(ssh, script, remote):
    status, data, errs = run(ssh, "python3 %s '%s'" % (script, remote))
    if status != 0:
        raise SystemExit("remote manifest failed: %s" % errs.strip()[:400])
    sizes = {}
    for line in data.splitlines():
        if "\t" not in line:
            continue
        size, rel = line.split("\t", 1)
        sizes[rel.replace(os.sep, "/")] = int(size)
    return sizes


def local_files(root, excludes):
    out = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if not d.startswith(".")]
        for name in filenames:
            if name.startswith("."):
                continue
            rel = os.path.relpath(os.path.join(dirpath, name), root)
            rel = rel.replace(os.sep, "/")
            if any(re.search(p, rel) for p in excludes):
                continue
            out.append(rel)
    return sorted(out)


def stream(ssh, local, remote, rels, label):
    total = sum(os.path.getsize(os.path.join(local, r)) for r in rels)
    status, _, errs = run(ssh, "mkdir -p '%s'" % remote)
    if status != 0:
        raise SystemExit("mkdir failed: %s" % errs[:200])
    chan = ssh.get_transport().open_session()
    chan.settimeout(1800)
    chan.exec_command("tar xf - -C '%s'" % remote)
    tf = tarfile.open(fileobj=ChannelFile(chan), mode="w|")
    start, done, sent = time.time(), 0, 0
    for rel in rels:
        path = os.path.join(local, rel)
        try:
            arc = posixpath.normpath(rel)
            if arc.startswith("..") or arc.startswith("/"):
                raise ValueError("unsafe relative path")
            tf.add(path, arcname=arc, recursive=False)
        except OSError as exc:
            print("[skip] %s: %s" % (rel, exc), flush=True)
        done += 1
        sent += os.path.getsize(path)
        if done % 200 == 0 or done == len(rels):
            el = time.time() - start
            print("[%s] %d/%d files, %.1f/%.1f MB, %.2f MB/s" % (
                label, done, len(rels), sent / 1e6, total / 1e6,
                sent / 1e6 / max(el, 0.01)), flush=True)
    tf.close()
    chan.shutdown_write()
    err = chan.makefile_stderr("rb").read().decode("utf-8", "replace")
    status = chan.recv_exit_status()
    chan.close()
    if status != 0 or err.strip():
        raise SystemExit("tar pipe failed (%d): %s" % (status, err[:400]))
    return total


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default=os.environ.get("DEVICE_HOST"))
    ap.add_argument("--port", type=int, default=22)
    ap.add_argument("--user", default="root")
    ap.add_argument("--password", default=os.environ.get("DEVICE_PASS"))
    ap.add_argument("--local", required=True)
    ap.add_argument("--remote", required=True)
    ap.add_argument("--exclude", action="append", default=[])
    ap.add_argument("--verify", type=int, default=6, help="spot-check N files by sha256")
    args = ap.parse_args()
    if not args.host:
        raise SystemExit("device host required (--host or DEVICE_HOST)")
    if not args.password:
        raise SystemExit("device password required (--password or DEVICE_PASS); none is stored here")

    ssh = connect(args.host, args.port, args.user, args.password)
    manifest_script = push_helper(ssh, "rr3_manifest.py", REMOTE_MANIFEST)
    hash_script = push_helper(ssh, "rr3_hash.py", REMOTE_HASH)
    have = remote_manifest(ssh, manifest_script, args.remote)
    local = local_files(args.local, args.exclude)
    todo = [r for r in local
            if have.get(r) != os.path.getsize(os.path.join(args.local, r))]
    print("remote has %d files; %d to send" % (len(have), len(todo)), flush=True)
    if not todo:
        print("already in sync")

    batches, current, current_bytes = [], [], 0
    for rel in todo:
        size = os.path.getsize(os.path.join(args.local, rel))
        if current and current_bytes + size > CHUNK_BYTES:
            batches.append(current)
            current, current_bytes = [], 0
        current.append(rel)
        current_bytes += size
    if current:
        batches.append(current)

    for i, batch in enumerate(batches, 1):
        stream(ssh, args.local, args.remote, batch, "batch%d/%d" % (i, len(batches)))

    final = remote_manifest(ssh, manifest_script, args.remote)
    missing = [r for r in local
               if final.get(r) != os.path.getsize(os.path.join(args.local, r))]
    print("verified: %d remote files, %d mismatched" % (len(final), len(missing)))
    for r in missing[:10]:
        print("  mismatch: %s" % r)

    import random
    candidates = [r for r, s in final.items() if s > 4096]
    random.seed(7)
    for rel in random.sample(candidates, min(args.verify, len(candidates))):
        status, digest, errs = run(ssh, "python3 %s '%s'" % (
            hash_script, posixpath.join(args.remote, rel)))
        if status != 0:
            print("  hash failed on %s: %s" % (rel, errs[:120]))
            continue
        h = hashlib.sha256()
        with open(os.path.join(args.local, rel), "rb") as f:
            for block in iter(lambda: f.read(1 << 20), b""):
                h.update(block)
        same = h.hexdigest() == digest.strip()
        print("  sha256 %-58s %s" % (rel, "MATCH" if same else "DIFFER"))
    ssh.close()
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
