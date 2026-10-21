#!/usr/bin/env python3
"""Fetch a remote file over SFTP to a local path."""
import os
import sys
import paramiko

HOST = os.environ.get("DEVICE_HOST", "192.168.31.192")
CREDS = {"ee47": ("root", "emuelec"), "rocknix": ("root", "rocknix")}


def main():
    remote = sys.argv[1]
    local = sys.argv[2]
    system = sys.argv[3] if len(sys.argv) > 3 else "ee47"
    user, password = CREDS[system]
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(HOST, username=user, password=password, timeout=20)
    sftp = client.open_sftp()
    sftp.get(remote, local)
    sftp.close()
    client.close()
    print("fetched %s -> %s (%d bytes)" % (remote, local, os.path.getsize(local)))


if __name__ == "__main__":
    main()
