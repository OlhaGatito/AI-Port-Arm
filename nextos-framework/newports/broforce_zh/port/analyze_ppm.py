#!/usr/bin/env python3
"""Report magenta share and the dominant colours of a P6 PPM capture."""
import sys
from collections import Counter


def main() -> int:
    path = sys.argv[1]
    with open(path, "rb") as fh:
        data = fh.read()
    # P6\n<w> <h>\n255\n
    parts = data.split(b"\n", 3)
    width, height = (int(v) for v in parts[1].split())
    pixels = parts[3]
    stride = width * 3
    total = width * height

    magenta = 0
    near_magenta = 0
    counter: Counter = Counter()
    for y in range(0, height, 2):
        row = pixels[y * stride:(y + 1) * stride]
        for x in range(0, width, 2):
            r = row[x * 3]
            g = row[x * 3 + 1]
            b = row[x * 3 + 2]
            counter[(r >> 4, g >> 4, b >> 4)] += 1
            if r > 200 and g < 60 and b > 200:
                magenta += 1
            elif r > 120 and g < 90 and b > 120 and r - g > 60 and b - g > 60:
                near_magenta += 1
    sampled = sum(counter.values())
    print("file=%s %dx%d sampled=%d" % (path, width, height, sampled))
    print("magenta=%.2f%%  near-magenta=%.2f%%"
          % (100.0 * magenta / sampled, 100.0 * near_magenta / sampled))
    print("distinct colour buckets=%d" % len(counter))
    print("top colours (r,g,b in 0-255 steps of 16):")
    for (r, g, b), n in counter.most_common(12):
        print("  #%02x%02x%02x  %5.2f%%"
              % (r * 16, g * 16, b * 16, 100.0 * n / sampled))
    return 0


if __name__ == "__main__":
    sys.exit(main())
