#!/usr/bin/env python3
"""Batch-upload the differing zh asset files + zh lib trio to the device.

Reads file list from zh_diff_files.txt (relative to apk/assets), SFTP-puts
each into /roms/ports/broforce_zh/assets/, then the three libs into lib/.
"""
import os
import sys
import time

import paramiko

HOST = os.environ.get("DEVICE_HOST", "192.168.31.192")
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LIST = os.path.join(ROOT, "tools", "zh_diff_files.txt")
LOCAL_ASSETS = os.path.join(ROOT, "apk", "assets")
LOCAL_LIB = os.path.join(ROOT, "apk", "lib", "arm64-v8a")
REMOTE = "/roms/ports/broforce_zh"

client = paramiko.SSHClient()
client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
client.connect(HOST, username="root", password="emuelec", timeout=20)
sftp = client.open_sftp()

def ensure_dir(path):
    parts = path.strip("/").split("/")
    cur = ""
    for p in parts:
        cur += "/" + p
        try:
            sftp.stat(cur)
        except IOError:
            sftp.mkdir(cur)

files = [l.strip() for l in open(LIST, encoding="utf-8") if l.strip()]
t0 = time.time()
done = 0
for rel in files:
    local = os.path.join(LOCAL_ASSETS, rel.replace("/", os.sep))
    rpath = REMOTE + "/assets/" + rel
    ensure_dir(os.path.dirname(rpath))
    sftp.put(local, rpath)
    done += 1
    if done % 25 == 0:
        print("%d/%d (%.1fs)" % (done, len(files), time.time() - t0), flush=True)
print("assets uploaded: %d files in %.1fs" % (done, time.time() - t0), flush=True)

for so in ["libmain.so", "libunity.so", "libil2cpp.so"]:
    sftp.put(os.path.join(LOCAL_LIB, so), REMOTE + "/lib/" + so + ".new")
    print("lib staged:", so, flush=True)

sftp.close()
client.close()
print("ALL DONE")
