#!/usr/bin/env python3
"""Compose body + face layer at (0,0) and dump PNGs for visual confirmation.

  python tools/composite_check.py apk/assets work 07 08 17
"""
import os
import sys

from PIL import Image

root, outdir = sys.argv[1], sys.argv[2]
dirs = sys.argv[3:] or ["07"]
os.makedirs(outdir, exist_ok=True)
for d in dirs:
    bp = os.path.join(root, d, "body_01.png")
    body = Image.open(bp).convert("RGBA")
    parts = []
    for pre in ("0", "1", "2", "3", "4", "5"):
        for kind in ("brow", "eye", "mouth"):
            p = os.path.join(root, d, "%s_%s.png" % (pre, kind))
            if os.path.exists(p):
                parts.append(p)
    fp = os.path.join(root, d, "0_face.png")
    if os.path.exists(fp):
        parts = [p for p in parts if os.path.basename(p).endswith("_face.png")]
        parts.append(fp)
    canvas = body.size
    for p in parts:
        w, h = Image.open(p).size
        canvas = (max(canvas[0], w), max(canvas[1], h))
    comp = Image.new("RGBA", canvas, (0, 0, 0, 0))
    comp.alpha_composite(body, (0, 0))
    for p in parts:
        im = Image.open(p).convert("RGBA")
        comp.alpha_composite(im, (0, 0))
    name = os.path.join(outdir, "composite_%s.png" % d)
    comp.save(name)
    flat = Image.new("RGB", canvas, (35, 38, 45))
    flat.paste(comp, (0, 0), comp)
    flat.save(os.path.join(outdir, "flat_%s.png" % d))
    print("%s  canvas=%s parts=%d" % (name, canvas, len(parts)))
