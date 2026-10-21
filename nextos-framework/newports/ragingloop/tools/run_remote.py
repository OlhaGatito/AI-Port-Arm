#!/usr/bin/env python3
"""Send a local shell script file verbatim to the handheld over SSH and print stdout.

Avoids Windows/Git-Bash quoting corruption of multi-line remote commands:
the file content is transferred as a single exec_command payload.
"""
import sys
import os
import paramiko

HOST = os.environ.get("DEVICE_HOST")
if not HOST:
    sys.exit("set DEVICE_HOST to the address supplied for this task")
CREDS = {"ee47": ("root", "emuelec"), "rocknix": ("root", "rocknix")}


def main():
    script_path = sys.argv[1]
    system = sys.argv[2] if len(sys.argv) > 2 else "ee47"
    user, password = CREDS[system]
    # utf-8 explicitly: these probe scripts carry Chinese comments, and the
    # Windows default (gbk) raises UnicodeDecodeError on them.
    with open(script_path, "r", newline="", encoding="utf-8") as fh:
        payload = fh.read().replace("\r\n", "\n").encode("utf-8")

    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(HOST, username=user, password=password, timeout=20)
    _, stdout, stderr = client.exec_command(payload, timeout=900)
    out = stdout.read().decode("utf-8", "replace")
    err = stderr.read().decode("utf-8", "replace")
    rc = stdout.channel.recv_exit_status()
    print(out)
    if err:
        print("--- stderr ---")
        print(err)
    print("--- rc=%d ---" % rc)
    client.close()


if __name__ == "__main__":
    main()
