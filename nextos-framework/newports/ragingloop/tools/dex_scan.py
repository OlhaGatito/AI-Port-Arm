#!/usr/bin/env python3
"""Minimal DEX table reader: string_ids / type_ids / class_defs / method_ids.

No disassembly, no external tooling. Usage:
  python dex_scan.py classes.dex [classes2.dex ...]
"""
import re
import struct
import sys

HDR = "<14I"          # from offset 56: string_ids_size .. data_off
HDR_OFF = 56


def uleb(buf, off):
    val = shift = 0
    while True:
        b = buf[off]
        off += 1
        val |= (b & 0x7F) << shift
        if not (b & 0x80):
            return val, off
        shift += 7


def read_cstring(buf, off):
    _, off = uleb(buf, off)
    end = buf.index(b"\x00", off)
    return buf[off:end].decode("utf-8", "replace")


def parse(path):
    buf = open(path, "rb").read()
    if buf[:4] != b"dex\n":
        print("%s: not a dex" % path)
        return None
    h = struct.unpack_from(HDR, buf, HDR_OFF)
    (sis, so, tis, to, _, _, _, _, mis, mo, cds, cdo, _, _) = h
    strings = [read_cstring(buf, struct.unpack_from("<I", buf, so + 4 * i)[0])
               for i in range(sis)]
    types = [strings[struct.unpack_from("<I", buf, to + 4 * i)[0]]
             for i in range(tis)]
    classes = []
    for i in range(cds):
        cidx = struct.unpack_from("<I", buf, cdo + 32 * i)[0]
        classes.append(types[cidx] if cidx < len(types) else "?")
    methods = []
    for i in range(mis):
        cidx, _, nidx = struct.unpack_from("<HHI", buf, mo + 8 * i)
        methods.append((types[cidx] if cidx < len(types) else "?",
                        strings[nidx] if nidx < len(strings) else "?"))
    print("== %s: strings=%d types=%d classes=%d methods=%d" %
          (path, len(strings), len(types), len(classes), len(methods)))
    return strings, types, classes, methods


MARKERS = {
    "surface": r"(?i)surfaceview|locksurface|ultrasurface",
    "gles": r"(?i)gles|gl_surface|opengles|eglcontext|eglgetdisplay",
    "mediaplayer": r"(?i)mediaplayer|videoview|exoplayer",
    "audio": r"(?i)audiotrack|soundpool|mediaextractor",
    "opensl": r"(?i)opensl",
    "ogg": r"(?i)vorbis|\.ogg",
    "webview": r"(?i)webview|webkit|javascript",
    "scriptvm": r"(?i)\blua\b|luaVM|monocsharp|clr|python",
    "engine": r"(?i)libgdx|andengine|cocos|unity|unreal|nintendo",
    "native": r"(?i)loadlibrary|system\.load|native",
    "image": r"(?i)bitmapsFactory|decodestream|createreversed|textureatlas",
    "assets": r"(?i)assetmanager|openrawresources|getassets",
    "save": r"(?i)sharedpreferences|openfileinput|save",
    "who": r"(?i)ragingloop|kemco|visualarts|human",
}


def main():
    for path in sys.argv[1:]:
        got = parse(path)
        if not got:
            continue
        strings, types, classes, methods = got
        pkgs = {}
        for c in classes:
            body = c.lstrip("L")
            parts = body.split("/")
            pkgs["/".join(parts[:3]) if len(parts) > 3 else body] = \
                pkgs.get("/".join(parts[:3]) if len(parts) > 3 else body, 0) + 1
        print("-- packages:")
        for k, v in sorted(pkgs.items(), key=lambda kv: -kv[1])[:25]:
            print("   %5d  %s" % (v, k))
        print("-- markers:")
        for name, pat in MARKERS.items():
            th = sorted({t for t in types if re.search(pat, t)})[:6]
            sh = sorted({s for s in strings if re.search(pat, s)})
            sh = [s for s in sh if len(s) < 90][:14]
            if th or sh:
                print("   [%s]" % name)
                for t in th:
                    print("        T %s" % t)
                for s in sh:
                    print("        S %s" % s)


if __name__ == "__main__":
    main()
