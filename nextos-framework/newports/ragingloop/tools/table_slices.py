#!/usr/bin/env python3
"""Slice the recovered 578-entry asset index table into families and show the
index ranges, so script command arguments can be mapped to real file paths.

  python table_slices.py logs/table-f-clinit.txt logs/table-slices.txt
"""
import collections
import re
import sys

src = sys.argv[1] if len(sys.argv) > 1 else "logs/table-f-clinit.txt"
dst = sys.argv[2] if len(sys.argv) > 2 else "logs/table-slices.txt"

rows = []
for line in open(src, encoding="utf-8", errors="replace"):
    m = re.match(r"^\s*(\d+)\s\s(.*)$", line)
    if m:
        rows.append(m.group(2).rstrip("\n"))


def fam(s):
    m = re.match(r"^(\d\d)/", s)
    if m:
        return "dir" + m.group(1)
    if re.match(r"^[ -~]+$", s):
        return re.sub(r"_?\d+$", "", re.sub(r"\d+", "N", s))
    return "text"


out = open(dst, "w", encoding="utf-8")
out.write("entries=%d\n\n== contiguous segments ==\n" % len(rows))
prev = None
start = 0
segs = []
for i, s in enumerate(rows):
    f = fam(s)
    if f != prev:
        if prev is not None:
            segs.append((start, i - 1, prev))
        prev = f
        start = i
segs.append((start, len(rows) - 1, prev))
for a, b, f in segs:
    out.write("%4d..%-4d n=%3d %-12s first=%s last=%s\n"
              % (a, b, b - a + 1, f, rows[a], rows[b]))

out.write("\n== per-family index sets ==\n")
byfam = collections.defaultdict(list)
for i, s in enumerate(rows):
    if re.match(r"^\d\d/", s):
        byfam[s.split("/")[0]].append((i, s))
    elif s.startswith("popup/") or s.startswith("emotion/") or \
            s.startswith("background/"):
        byfam[s.split("/")[0]].append((i, s))
for k in sorted(byfam):
    idx = byfam[k]
    out.write("\n[%s] n=%d idx %d..%d\n" % (k, len(idx), idx[0][0], idx[-1][0]))
    kinds = collections.defaultdict(list)
    for i, s in idx:
        base = s.split("/")[1]
        kinds[re.sub(r"^\d+_", "", base).split("_")[0]].append(i)
    for kk, ii in sorted(kinds.items()):
        out.write("    %-8s n=%3d idx=%s\n" % (kk, len(ii), ii[:8]))
    out.write("    ordered head: %s\n" % ", ".join(s for _, s in idx[:6]))
    out.write("    ordered tail: %s\n" % ", ".join(s for _, s in idx[-6:]))
out.close()
print("wrote %s" % dst)
