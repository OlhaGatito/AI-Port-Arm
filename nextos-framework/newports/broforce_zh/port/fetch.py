#!/usr/bin/env python3
"""Fetch a file from the EmuELEC test device over SSH (base64 transport)."""
import base64 as b64
import sys

import paramiko

HOST, USER, PASS = "192.168.31.218", "root", "emuelec"


def main() -> int:
    remote, local = sys.argv[1], sys.argv[2]
    cli = paramiko.SSHClient()
    cli.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    cli.connect(HOST, username=USER, password=PASS, timeout=10)
    try:
        _, out, err = cli.exec_command(
            "openssl base64 -A -in " + remote, timeout=180)
        raw = out.read()
        e = err.read().decode("utf-8", "replace")
        rc = out.channel.recv_exit_status()
        print("[fetch] rc=%s b64len=%d stderr=%r" % (rc, len(raw), e[:200]))
        data = b64.b64decode(raw)
        with open(local, "wb") as fh:
            fh.write(data)
        print("saved", local, len(data), "bytes")
        return 0
    finally:
        cli.close()


if __name__ == "__main__":
    sys.exit(main())
