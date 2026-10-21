#!/usr/bin/env python3
"""Crude il2cpp metadata string survey: pull identifier-looking ASCII strings
from global-metadata.dat and libil2cpp.so, grouped by keyword."""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PORT = os.path.dirname(HERE)
APK = os.path.join(PORT, "apk")

META = os.path.join(APK, "assets", "bin", "Data", "Managed", "Metadata",
                    "global-metadata.dat")
IL2CPP = os.path.join(APK, "lib", "arm64-v8a", "libil2cpp.so")

IDENT = re.compile(rb"[A-Za-z_][A-Za-z0-9_.`<>]{3,70}")


def names(path, limit=None):
    with open(path, "rb") as fh:
        data = fh.read() if limit is None else fh.read(limit)
    return set(m.group().decode("utf-8", "replace") for m in IDENT.finditer(data))


def main():
    kws = sys.argv[1:] or ["VideoPlayer", "Video", "AVPro", "Prepare",
                           "MediaPlayer", "Login", "Sdk", "Hotfix"]
    for label, path in (("metadata", META), ("libil2cpp", IL2CPP)):
        if not os.path.exists(path):
            print("missing", path)
            continue
        ns = names(path)
        print("=== %s: %d identifiers ===" % (label, len(ns)))
        for k in kws:
            hits = sorted(n for n in ns if k in n)
            print("-- %s (%d) --" % (k, len(hits)))
            print(hits[:40])


if __name__ == "__main__":
    main()
