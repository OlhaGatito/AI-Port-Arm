#!/usr/bin/env python3
"""Upload one local file to the handheld via SFTP: write to <remote>.next, chmod,
then atomic mv on the device. Progress goes to stderr so a stalled transfer shows.

Usage: put_file.py <local> <remote-dir> [filename]
IP/credentials come from DEVICE_HOST / DEVICE_SYSTEM (never hardcoded here).
"""
import hashlib
import os
import sys
import time

import paramiko

HOST = os.environ.get("DEVICE_HOST")   # must come from the current task
if not HOST:
    raise SystemExit("set DEVICE_HOST")
CREDS = {"ee47": ("root", "emuelec"), "rocknix": ("root", "rocknix")}
SYSTEM = os.environ.get("DEVICE_SYSTEM", "rocknix")


def main():
    local, remote_dir = sys.argv[1], sys.argv[2]
    name = sys.argv[3] if len(sys.argv) > 3 else os.path.basename(local)
    print("argv resolved: local=%r remote_dir=%r name=%r host=%s"
          % (local, remote_dir, name, HOST), file=sys.stderr)

    # Open locally first: a bad local path must fail as a local error, not an
    # ambiguous SFTP ENOENT about the remote file.
    with open(local, "rb") as fh:
        digest = hashlib.sha256()
        for chunk in iter(lambda: fh.read(1 << 22), b""):
            digest.update(chunk)
    local_sum = digest.hexdigest()
    size = os.path.getsize(local)
    t0 = time.time()
    print("local sha256=%s size=%.1f MB (hashed in %.1fs)"
          % (local_sum, size / 1048576.0, time.time() - t0), file=sys.stderr)

    user, password = CREDS[SYSTEM]
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(HOST, username=user, password=password, timeout=20)
    sftp = client.open_sftp()
    remote_tmp = posixpath_join(remote_dir, name + ".next")
    last = [0.0]

    def progress(done, total):
        if time.time() - last[0] >= 5.0:
            last[0] = time.time()
            print("  %6.1f%%  %d/%d MB" % (done * 100.0 / total, done >> 20, total >> 20),
                  file=sys.stderr, flush=True)

    sftp.put(local, remote_tmp, callback=progress)
    sftp.chmod(remote_tmp, 0o644)
    sftp.close()
    print("put done in %.1fs, verifying atomic replace" % (time.time() - t0),
          file=sys.stderr)

    dst = posixpath_join(remote_dir, name)
    _, stdout, stderr = client.exec_command(
        "mv -f '%s' '%s' && sha256sum '%s' && ls -l '%s'"
        % (remote_tmp, dst, dst, dst), timeout=900)
    print(stdout.read().decode("utf-8", "replace"))
    err = stderr.read().decode("utf-8", "replace")
    if err:
        print("--- stderr ---\n" + err)
    print("expected sha256=%s elapsed=%.1fs" % (local_sum, time.time() - t0),
          file=sys.stderr)
    client.close()


def posixpath_join(a, b):
    return a.rstrip("/") + "/" + b


if __name__ == "__main__":
    main()
