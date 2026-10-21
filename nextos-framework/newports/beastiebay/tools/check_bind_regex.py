#!/usr/bin/env python3
"""Check that contract_gap's binding regex actually sees the grouped bindings.

The run28 bindings are written as fp2_jni_bind(U, "name", ...) under a
`static const char *U` alias. If the tool's pattern only accepted a literal
class name it would report the whole group as missing -- and a gap report that
hides real gaps is worse than none, because it looks authoritative.
"""
import re
import sys

BIND_RE = re.compile(
    r'fp2_jni_bind\(\s*(?:"[^"]+"|[A-Za-z_]\w*)\s*,\s*"([^"]+)"')

EXPECT = ["getWidth", "getHeight", "isTablet", "isFullscreen", "isDebug",
          "fileExists", "hasPermission", "checkPlayServices",
          "getLicenseCheck", "hasIntent", "getErrMsg", "getSignature"]


def main():
    src = open(sys.argv[1], encoding="utf-8", errors="replace").read()
    names = set(BIND_RE.findall(src))
    print("binding matches: %d distinct method names" % len(names))
    bad = [n for n in EXPECT if n not in names]
    for n in EXPECT:
        print("  %-24s %s" % (n, "ok" if n in names else "MISSING"))
    if bad:
        print("--- regex is not seeing %d expected binding(s) ---" % len(bad))
        return 1
    print("--- regex sees every grouped binding ---")
    return 0


if __name__ == "__main__":
    sys.exit(main())
