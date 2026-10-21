#!/usr/bin/env python3
"""Census of the ragingloop script command language.

Scans every assets/script/*.txt, splits lines into '#Name(args)' commands plus
bare text, and reports each command with its argument-count distribution and
one example. This is the evidence used to size a native interpreter.
"""
import collections
import glob
import os
import re
import sys

d = sys.argv[1] if len(sys.argv) > 1 else "apk/assets/script"
cmd_re = re.compile(r"^#([A-Za-z_][A-Za-z0-9_]*)\((.*)\)\s*$")
bare_re = re.compile(r"^#([A-Za-z_][A-Za-z0-9_]*)\s*$")

argc = collections.defaultdict(collections.Counter)
files = collections.defaultdict(set)
examples = {}
total_lines = text_lines = cmd_lines = other = 0

for path in sorted(glob.glob(os.path.join(d, "*.txt"))):
    name = os.path.basename(path)
    with open(path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            line = line.rstrip("\r\n")
            total_lines += 1
            m = cmd_re.match(line)
            if m:
                c, args = m.group(1), m.group(2)
                n = 0 if args == "" else len(args.split(","))
                argc[c][n] += 1
                files[c].add(name)
                examples.setdefault(c, line)
                cmd_lines += 1
                continue
            m = bare_re.match(line)
            if m:
                argc[m.group(1)][0] += 1
                files[m.group(1)].add(name)
                examples.setdefault(m.group(1), line)
                cmd_lines += 1
                continue
            if line.strip() == "":
                continue
            text_lines += 1

out = sys.stdout
out.write("files=%d total_lines=%d command_lines=%d text_lines=%d\n\n" %
          (len(glob.glob(os.path.join(d, "*.txt"))), total_lines, cmd_lines, text_lines))
out.write("%-22s %7s  %-16s %s\n" % ("COMMAND", "COUNT", "ARGC(xN)", "FILES"))
for c, counter in sorted(argc.items(), key=lambda kv: -sum(kv[1].values())):
    tot = sum(counter.values())
    argd = ",".join("%d:%d" % (k, v) for k, v in sorted(counter.items()))
    fl = len(files[c])
    out.write("%-22s %7d  %-16s %d\n" % (c, tot, argd, fl))

out.write("\n== examples ==\n")
for c in sorted(argc):
    ex = examples[c].replace("\t", " ")
    out.write("%-22s %s\n" % (c, ex[:150]))
