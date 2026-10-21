#!/usr/bin/env python3
"""Locate the face-part canvas on the body canvas by measurement, not guesswork.

#ImgBustAdv(slot,bustId,x,y,..) positions the body; #ImgFaceAdv(slot,brow,eye,mouth,
acce,emo) carries NO coordinates, so the engine must anchor the small face canvas
(itself and every part share one size) onto the body somehow. Candidates:
  H0 top-left   (0, 0)
  H1 centred-x  ((bw-cw)//2, 0)
Discriminator: the body artwork has a skin-coloured face area; the union of the
part alpha boxes must land on it. Print IoU per hypothesis for every character dir.
"""
import glob
import os
import sys

from PIL import Image

sys.stdout.reconfigure(encoding="utf-8", errors="replace")


def alphabox(path, thr=12):
    im = Image.open(path).convert("RGBA")
    a = im.split()[3]
    box = a.point(lambda v: 255 if v > thr else 0).getbbox()
    return im.size, box


def skin_bbox(path):
    """Bounding box of likely anime-skin pixels in the upper half of the bust."""
    im = Image.open(path).convert("RGBA")
    w, h = im.size
    px = im.load()
    minx, miny, maxx, maxy = w, h, -1, -1
    import colorsys
    for y in range(0, h // 2, 2):
        for x in range(0, w, 2):
            r, g, b, a = px[x, y]
            if a < 40:
                continue
            hh, ss, vv = colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)
            hue = hh * 360
            if 6 <= hue <= 42 and 0.10 <= ss <= 0.55 and vv >= 0.72:
                minx = min(minx, x); maxx = max(maxx, x)
                miny = min(miny, y); maxy = max(maxy, y)
    return im.size, (minx, miny, maxx, maxy) if maxx >= 0 else None


def iou(a, b):
    if not a or not b:
        return 0.0
    ix = max(0, min(a[2], b[2]) - max(a[0], b[0]))
    iy = max(0, min(a[3], b[3]) - max(a[1], b[1]))
    inter = ix * iy
    ar = (a[2] - a[0]) * (a[3] - a[1])
    br = (b[2] - b[0]) * (b[3] - b[1])
    return inter / float(ar + br - inter) if (ar + br - inter) > 0 else 0.0


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else "apk/assets"
    print("%-4s %-11s %-9s %-22s %-18s %6s %6s" %
          ("dir", "body", "canvas", "parts-union", "skin-box", "IoU0", "IoU1"))
    for d in sorted(os.listdir(root)):
        bp = os.path.join(root, d, "body_01.png")
        if not (len(d) == 2 and d.isdigit() and os.path.exists(bp)):
            continue
        parts = [p for p in glob.glob(os.path.join(root, d, "*.png"))
                 if os.path.basename(p).split("_")[0].isdigit()
                 and not p.endswith(("body_01.png", "body_02.png"))]
        parts = [p for p in parts if os.path.basename(p) not in
                 ("black.png", "red.png")]
        if not parts:
            continue
        csize = None
        ux = uy = 10 ** 9
        lx = ly = -1
        for p in parts:
            size, box = alphabox(p)
            if not box:
                continue
            if csize and size != csize:
                print("   %s: MIXED canvas size %s vs %s" % (d, size, csize))
            csize = size
            ux, uy = min(ux, box[0]), min(uy, box[1])
            lx, ly = max(lx, box[2]), max(ly, box[3])
        bsize, _ = alphabox(bp)
        _, skin = skin_bbox(bp)
        bw, bh = bsize
        cw, ch = csize or bsize
        u0 = (ux, uy, lx, ly)
        u1 = (ux + (bw - cw) // 2, uy, lx + (bw - cw) // 2, ly)
        print("%-4s %-11s %-9s %-22s %-18s %6.3f %6.3f" %
              (d, "%dx%d" % bsize, "%dx%d" % csize, str(u0), str(skin),
               iou(u0, skin), iou(u1, skin)))


main()
