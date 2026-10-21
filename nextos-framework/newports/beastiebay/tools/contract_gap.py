#!/usr/bin/env python3
"""Diff the Kairosoft plugin layer in the APK against what the port binds.

Why this replaces guesswork
---------------------------
The port's JNI layer grew one stub per observed call, so it is always behind
the real contract: on run28, 98 of Utility's methods had never been bound, and
every one of them was answered by the generic stub. For a PREDICATE that
asserts "false" rather than "unknown", which is how BootForm's start-up
self-check ended up in STATE_ERROR and reported error 100.

The APK's classes.dex is the authoritative list, so the gap between the two is
computable instead of guessable. This reports, per class, the methods the dex
declares that no fp2_jni_bind and no dispatch-level name comparison covers.

Usage:
  contract_gap.py apk/classes.dex port/src/jni.c
  contract_gap.py apk/classes.dex port/src/jni.c --class Utility
"""
import argparse
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dex_probe import Dex  # noqa: E402

INTERESTING = {
    "Lkairo/android/plugin/Utility;",
    "Lkairo/android/plugin/Config;",
    "Lkairo/android/plugin/ad/AdMobView;",
    "Lkairo/android/plugin/ad/Interstitial;",
    "Lkairo/android/plugin/io/Preference;",
}

# The class argument may be a literal or an alias (the run28 bindings sit under
# a `static const char *U = "kairo/android/plugin/Utility";`), so accept both.
# The method argument is always a literal. Verified by check_bind_regex.py --
# an earlier pattern with an over-greedy alternation matched nothing at all and
# reported every binding as missing.
BIND_RE = re.compile(r'fp2_jni_bind\(\s*(?:"[^"]+"|[A-Za-z_]\w*)\s*,\s*"([^"]+)"')
# dispatch()-level handlers compare the method name directly, so they count as
# covered even without an explicit binding.
NAME_RE = re.compile(r'strcmp\(\s*m->name\s*,\s*"([^"]+)"')
# j_<Class>_<method> style helpers are a third way a name gets handled.
HELPER_RE = re.compile(r'\bj_[A-Za-z0-9]+_([A-Za-z0-9_]+)\s*\(')


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("dex")
    ap.add_argument("source")
    ap.add_argument("--klass", help="restrict to one class name (substring)")
    ap.add_argument("--only-classes", action="store_true",
                    help="only the Kairosoft plugin classes")
    args = ap.parse_args()

    dex = Dex(open(args.dex, "rb").read())
    src = open(args.source, encoding="utf-8", errors="replace").read()
    covered = set(BIND_RE.findall(src))
    covered |= set(NAME_RE.findall(src))
    covered |= set(HELPER_RE.findall(src))

    def proto(index):
        off = dex.proto_ids_off + index * 12
        _, ret, params_off = struct.unpack_from("<III", dex.blob, off)
        params = []
        if params_off:
            count, = struct.unpack_from("<I", dex.blob, params_off)
            for k in range(count):
                t, = struct.unpack_from("<H", dex.blob, params_off + 4 + k * 2)
                params.append(dex.type_name(t))
        return dex.type_name(ret), params

    total_missing = 0
    for cls in dex.classes():
        name = cls["name"]
        if args.only_classes and name not in INTERESTING:
            continue
        if args.klass and args.klass not in name:
            continue
        sf, i_f, dm, vm = dex.class_members(cls["class_data_off"])
        missing = []
        for idx, _acc, _code in dm + vm:
            _, mname, pidx = dex.method(idx)
            if mname.startswith("access$") or mname in ("<clinit>", "<init>"):
                continue
            if mname in covered:
                continue
            ret, params = proto(pidx)
            missing.append((mname, "".join(params), ret))
        if not missing:
            continue
        total_missing += len(missing)
        print("=== %s : %d unbound ===" % (name, len(missing)))
        for mname, params, ret in sorted(missing):
            print("    %-32s (%s)%s" % (mname, params, ret))
    print("\n--- %d method(s) declared in the dex but not handled ---"
          % total_missing)
    return 0


if __name__ == "__main__":
    sys.exit(main())
