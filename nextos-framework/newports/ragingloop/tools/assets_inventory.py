#!/usr/bin/env python3
"""Asset inventory for the ragingloop APK unpack: per-dir counts, extension
totals/sizes, and the RestoreToGame payload breakdown."""
import collections
import os
import sys

root = sys.argv[1] if len(sys.argv) > 1 else "apk/assets"
bydir = collections.Counter()
ext = collections.Counter()
extsize = collections.Counter()
rg = collections.Counter()

for dp, dn, fn in os.walk(root):
    rel = os.path.relpath(dp, root).replace(os.sep, "/")
    top = rel.split("/")[0] if rel != "." else "(root)"
    for f in fn:
        p = os.path.join(dp, f)
        sz = os.path.getsize(p)
        e = os.path.splitext(f)[1].lower()
        bydir[top] += 1
        ext[e] += 1
        extsize[e] += sz
        if top == "RestoreToGame":
            sub = rel[len("RestoreToGame"):].strip("/")
            rg[sub.split("/")[0] if sub else "(own)"] += 1

print("== per top-level dir (files) ==")
for k, v in sorted(bydir.items(), key=lambda kv: -kv[1]):
    print("%7d  %s" % (v, k))
print("\n== extensions ==")
for k, v in ext.most_common():
    print("%7d %10.1f MB  %s" % (v, extsize[k] / 1048576.0, k or "(none)"))
print("\n== RestoreToGame breakdown ==")
for k, v in rg.most_common(30):
    print("%7d  RestoreToGame/%s" % (v, k))
