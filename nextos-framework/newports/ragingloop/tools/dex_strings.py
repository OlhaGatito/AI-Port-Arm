#!/usr/bin/env python3
"""Extract resource-path and command tokens from a DEX string pool.

The goal is the engine's filename mapping tables (which index maps to which
asset) without decompiling bytecode: those live as plain strings.

  python dex_strings.py apk/classes.dex
"""
import re
import sys

sys.path.insert(0, __import__("os").path.dirname(__file__))
from dex_scan import parse  # noqa: E402

FAMILIES = {
    "command": r"^#[A-Za-z]",
    "bg": r"^bg_\d+",
    "char": r"^bust_|^face_|^_?bust|^char",
    "png": r"\.png$",
    "jpg": r"\.jpe?g$",
    "ogg": r"\.ogg$",
    "mp4": r"\.mp4$",
    "txt": r"\.txt$",
    "xml": r"\.xml$",
    "csv": r"\.csv$",
    "lmp": r"\.lmp$",
    "path": r"^(script|sound|voice|background|popup|thumb|data)/",
    "asset": r"(?i)asset|external|getExternalFiles|files/",
    "pref": r"(?i)^RL|shared|pref",
    "screen": r"(?i)density|resolution|width|height|scale|displaymetrics",
}


def main():
    path = sys.argv[1]
    got = parse(path)
    if not got:
        return
    strings, types, classes, methods = got
    blob = "\n".join(strings)
    for name, pat in FAMILIES.items():
        rx = re.compile(pat)
        hits = sorted({s for s in strings if rx.search(s) and len(s) < 80})
        if not hits:
            continue
        print("== [%s] %d" % (name, len(hits)))
        for h in hits[:120]:
            print("   %s" % h)
        if len(hits) > 120:
            print("   ... (%d more)" % (len(hits) - 120))
    print("\n== kemco/ragingloop classes ==")
    for c in sorted(x for x in classes if "kemco/ragingloop" in x):
        print("   %s" % c)


main()
