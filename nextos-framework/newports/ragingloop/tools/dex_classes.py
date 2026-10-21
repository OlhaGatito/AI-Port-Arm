#!/usr/bin/env python3
"""DEX structure dumper for engine triage.

Lists class definitions (with superclass) and the method names owned by each
class, filtered by a descriptor prefix. Also reports which classes reference
the ADV command tokens, which is how the interpreter's dispatch site is found.

  python dex_classes.py apk/classes.dex kemco/ragingloop
"""
import re
import struct
import sys

sys.path.insert(0, os_dir := __import__("os").path.dirname(__file__))
from dex_scan import parse  # noqa: E402


def main():
    path, prefix = sys.argv[1], sys.argv[2]
    got = parse(path)
    if not got:
        return
    strings, types, classes, methods = got

    # class_def_item: class_idx, access_flags, superclass_idx, interfaces_off,
    # source_file_idx, annotations_off, class_data_off, static_values_off
    buf = open(path, "rb").read()
    h = struct.unpack_from("<14I", buf, 56)
    cds, cdo = h[10], h[11]
    super_of = {}
    for i in range(cds):
        base = cdo + 32 * i
        cidx, _, sidx = struct.unpack_from("<3I", buf, base)
        super_of[types[cidx]] = types[sidx] if sidx < len(types) else "?"

    by_class = {}
    for t, n in methods:
        by_class.setdefault(t, []).append(n)

    want = [c for c in classes if prefix.strip("L") in c]
    print("matching classes=%d" % len(want))
    for c in sorted(want):
        ms = sorted(set(by_class.get(c, [])))
        print("\n%s  : %s" % (c, super_of.get(c, "?")))
        print("   methods(%d): %s" % (len(ms), ", ".join(ms[:60])))

    # Where do the command tokens live?
    toks = sorted({m for _, m in methods if False})
    print("\n== command token reference sites ==")
    str_set = set(strings)
    for t in ("MesSta", "ImgBack", "ImgFaceAdv", "WipeIn", "Select", "sounddef"):
        print("   %-12s in string pool: %s" % (t, t in str_set))


main()
