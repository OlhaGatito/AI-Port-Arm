#!/usr/bin/env python3
"""Upload a file to the EmuELEC test device over SSH (raw binary via cat)."""
import sys

import paramiko

HOST, USER, PASS = "192.168.31.218", "root", "emuelec"


def main() -> int:
    local, remote = sys.argv[1], sys.argv[2]
    with open(local, "rb") as fh:
        data = fh.read()

    cli = paramiko.SSHClient()
    cli.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    cli.connect(HOST, username=USER, password=PASS, timeout=10)
    try:
        ch = cli.get_transport().open_session()
        ch.settimeout(120)
        ch.exec_command("cat > " + remote)
        sent = 0
        while sent < len(data):
            try:
                n = ch.send(data[sent:sent + 8192])
            except OSError as exc:
                print("[upload] send failed at %d: %r" % (sent, exc))
                break
            if n == 0:
                print("[upload] window full at %d" % sent)
                ch.recv(1024)
                continue
            sent += n
        ch.shutdown_write()
        print("[upload] sent=%d/%d status=%s" % (sent, len(data), ch.recv_exit_status()))
        _, out, err = cli.exec_command(
            "chmod a+x " + remote + " && md5sum " + remote + " && ls -la " + remote,
            timeout=60)
        print(out.read().decode("utf-8", "replace"))
        e = err.read().decode("utf-8", "replace")
        if e:
            print("[stderr]", e)
        return 0
    finally:
        cli.close()


if __name__ == "__main__":
    sys.exit(main())
