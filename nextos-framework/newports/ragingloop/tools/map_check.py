#!/usr/bin/env python3
"""Cross-check script arguments against the real asset filenames.

Purpose: prove (or disprove) the index->file mapping the Java engine uses, so a
native interpreter can reuse it. Reads assets/script/*.txt and the asset tree.

  python map_check.py apk
"""
import collections
import glob
import os
import re
import sys

root = sys.argv[1] if len(sys.argv) > 1 else "apk"
A = os.path.join(root, "assets")
cmd_re = re.compile(r"^#([A-Za-z_][A-Za-z0-9_]*)\((.*)\)\s*$")

args = collections.defaultdict(list)
for path in glob.glob(os.path.join(A, "script", "*.txt")):
    with open(path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            m = cmd_re.match(line.rstrip("\r\n"))
            if m:
                args[m.group(1)].append(
                    [a.strip() for a in m.group(2).split(",")])


def rng(vals):
    return "%s..%s (n=%d, uniq=%d)" % (min(vals), max(vals), len(vals),
                                       len(set(vals)))


def listing(rel):
    d = os.path.join(A, rel)
    if not os.path.isdir(d):
        return []
    return sorted(os.listdir(d))


print("== numeric argument ranges ==")
for c in ("ImgBack", "ImgBustAdv", "ImgScale", "ImgFaceAdv", "ImgItem",
          "ImgBustXY", "ImgBustOffAdv", "ImgAlpha", "ImgAnim", "MoveTo",
          "Bgm", "BgmIn", "Se", "MesWin", "MesType", "MesColor", "WipeIn",
          "WipeOut", "Flash", "Quake", "Wait", "Ret", "JumpC", "CmpC", "CmpS",
          "FlagSOFF", "FlagCON"):
    rows = args.get(c, [])
    if not rows:
        continue
    cols = collections.defaultdict(list)
    for r in rows:
        for i, v in enumerate(r):
            try:
                cols[i].append(int(v))
            except ValueError:
                cols[i].append(v)
    desc = "  ".join("p%d=%s" % (i, (rng(v) if v and isinstance(v[0], int)
                                     else sorted(set(map(str, v)))[:6]))
                     for i, v in sorted(cols.items()))
    print("%-14s %s" % (c, desc))

print("\n== background dir ==")
bg = listing("background")
print("files=%d  first=%s last=%s" % (len(bg), bg[0] if bg else "-",
                                      bg[-1] if bg else "-"))
nums = {int(re.search(r"(\d+)", b).group(1)) for b in bg if re.search(r"\d+", b)}
want = set(args["ImgBack"][0][0] and [int(r[0]) for r in args["ImgBack"]])
print("script ImgBack ids not present in dir: %s" %
      sorted(x for x in want if x not in nums)[:20])

print("\n== character dirs ==")
for d in [x for x in listing(".") if len(x) == 2 and x.isdigit()]:
    files = listing(d)
    kinds = collections.Counter(re.sub(r"^\d+_", "", f).split("_")[0]
                                for f in files if f.endswith(".png"))
    print("  %s: %3d files  kinds=%s" % (d, len(files), dict(kinds)))

print("\n== popup / thumb ==")
print("popup=%d  thumb=%d" % (len(listing("popup")), len(listing("thumb"))))
pk = collections.Counter(re.sub(r"\d+", "N", f) for f in listing("popup"))
print("popup patterns: %s" % dict(pk))
tk = collections.Counter(re.sub(r"\d+", "N", f) for f in listing("thumb"))
print("thumb patterns: %s" % dict(list(tk.items())[:12]))

print("\n== voice / sound ==")
R = os.path.join(A, "RestoreToGame", "external_data", "files")
voice = sorted(os.listdir(os.path.join(R, "voice"))) if os.path.isdir(
    os.path.join(R, "voice")) else []
sound = sorted(os.listdir(os.path.join(R, "sound"))) if os.path.isdir(
    os.path.join(R, "sound")) else []
print("voice=%d sound=%d" % (len(voice), len(sound)))
print("voice sample: %s" % voice[:6])
vid = collections.Counter(re.sub(r"\d", "N", f) for f in voice)
print("voice patterns: %s" % dict(list(vid.items())[:10]))
print("sound: %s" % sound[:12])

print("\n== MesSta ids vs voice files ==")
ms = {r[0] for r in args["MesSta"] if len(r) > 1 and r[0] and r[0] != "none"}
vset = {os.path.splitext(f)[0] for f in voice}
print("distinct MesSta ids=%d, present in voice/=%d, missing(examples)=%s" %
      (len(ms), sum(1 for x in ms if x in vset),
       sorted(x for x in ms if x not in vset)[:12]))

print("\n== sounddef table ==")
sd = os.path.join(A, "script", "sounddef.txt")
if os.path.exists(sd):
    kinds = collections.Counter()
    rows = []
    with open(sd, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            p = line.strip().split(",")
            if len(p) >= 4:
                kinds[p[0]] += 1
                rows.append(p)
    print("kinds=%s total=%d" % (dict(kinds), len(rows)))
    idx = [int(r[3]) for r in rows if r[3].lstrip("-").isdigit()]
    print("index range %d..%d, dup=%s" % (min(idx), max(idx),
                                          [i for i, c in
                                           collections.Counter(idx).items()
                                           if c > 1][:10]))
    se = [r for r in rows if r[0] == "SE"]
    print("SE sample: %s" % se[:6])
    used_se = {int(r[0]) for r in args["Se"] if r[0].lstrip("-").isdigit()}
    have = {int(r[3]) for r in rows if r[3].lstrip("-").isdigit()}
    print("Se ids used=%d missing from table=%s" %
          (len(used_se), sorted(used_se - have)[:12]))
    used_bgm = {int(r[0]) for r in args["Bgm"] if r[0].lstrip("-").isdigit()}
    print("Bgm ids used=%d missing from table=%s" %
          (len(used_bgm), sorted(used_bgm - have)[:12]))
