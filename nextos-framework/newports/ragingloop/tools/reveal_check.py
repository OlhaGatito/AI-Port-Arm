#!/usr/bin/env python3
"""Count message-box text pixels per snapshot: a typewriter reveal must grow
frame over frame and then plateau, where the old single-frame draw jumped from
0 to the full count in one step.

Usage: reveal_check.py <dir with shot*.bmp>
"""
import os
import re
import sys
from PIL import Image


def main():
    d = sys.argv[1]
    shots = [f for f in os.listdir(d) if re.match(r"shot\d+_f(\d+)\.bmp$", f)]
    shots.sort(key=lambda f: int(re.search(r"_f(\d+)\.bmp$", f).group(1)))
    prev = None
    for f in shots:
        im = Image.open(os.path.join(d, f)).convert("RGB")
        px = im.load()
        n = 0
        minx = im.width
        maxx = -1
        miny = im.height
        maxy = -1
        for y in range(540, min(704, im.height)):
            for x in range(88, min(1196, im.width)):
                r, g, b = px[x, y]
                if r > 190 and g > 190 and b > 190:
                    n += 1
                    if x < minx: minx = x
                    if x > maxx: maxx = x
                    if y < miny: miny = y
                    if y > maxy: maxy = y
        frame = re.search(r"_f(\d+)\.bmp$", f).group(1)
        delta = "-" if prev is None else "%+d" % (n - prev)
        box = "bbox x=%d..%d y=%d..%d" % (minx, maxx, miny, maxy) if n else "no ink"
        print("f%-6s white=%-7d %-14s %s" % (frame, n, delta, box))
        prev = n


if __name__ == "__main__":
    main()
