#!/usr/bin/env python3
"""Pull one remote file from the handheld over SFTP.

Usage: get_file.py <remote-path> <local-path>
IP/credentials come from DEVICE_HOST / DEVICE_SYSTEM (never hardcoded here).
"""
import os
import sys
import time

import paramiko

HOST = os.environ.get("DEVICE_HOST")
if not HOST:
    raise SystemExit("set DEVICE_HOST")
CREDS = {"ee47": ("root", "emuelec"), "rocknix": ("root", "rocknix")}
SYSTEM = os.environ.get("DEVICE_SYSTEM", "rocknix")


def main():
    remote, local = sys.argv[1], sys.argv[2]
    user, password = CREDS[SYSTEM]
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(HOST, username=user, password=password, timeout=20)
    sftp = client.open_sftp()
    size = sftp.stat(remote).st_size
    os.makedirs(os.path.dirname(os.path.abspath(local)), exist_ok=True)
    t0 = time.time()
    last = [0.0]

    def progress(done, total):
        if time.time() - last[0] >= 5.0:
            last[0] = time.time()
            print("  %6.1f%%  %d/%d MB" % (done * 100.0 / total, done >> 20, total >> 20),
                  file=sys.stderr, flush=True)

    sftp.get(remote, local, callback=progress)
    got = os.path.getsize(local)
    sftp.close()
    client.close()
    print("get %s -> %s  %d/%d bytes in %.1fs"
          % (remote, local, got, size, time.time() - t0), file=sys.stderr)
    if got != size:
        raise SystemExit("size mismatch")


if __name__ == "__main__":
    main()
