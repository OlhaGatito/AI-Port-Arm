#!/usr/bin/env python3
"""Does the stripped libil2cpp.so export anything near the throw helper?

The instrument plan for run39h was to hook the exception helper by NAME.
That only works if the helper is in .dynsym.  This script answers the
question directly instead of guessing: it walks the section table, reads
.dynsym, and reports every symbol whose value falls in a window around the
address the offline disassembly pointed at.

Why bother: run33 cost a SIGSEGV because an address was assumed rather than
verified.  The same discipline applies to the opposite mistake -- assuming
the symbol is NOT there and hardcoding an RVA.  Either answer has to come
from the binary, not from expectation.
"""
import struct
import sys


def sections(blob):
    e_shoff, = struct.unpack_from("<Q", blob, 0x28)
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from("<HHH", blob, 0x3A)
    out = []
    for i in range(e_shnum):
        off = e_shoff + i * e_shentsize
        name_off, st, fl, ad, of, sz = struct.unpack_from("<IIQQQQ", blob, off)
        out.append((name_off, st, fl, ad, of, sz))
    return out, e_shstrndx


def main(path, lo, hi):
    blob = open(path, "rb").read()
    secs, shstrndx = sections(blob)
    stroff = secs[shstrndx][4]

    def nm(o):
        e = blob.index(b"\x00", stroff + o)
        return blob[stroff + o:e].decode()

    dynsym = next(s for s in secs if nm(s[0]) == ".dynsym")
    dynstr = next(s for s in secs if nm(s[0]) == ".dynstr")
    strtab = dynstr[4]
    n = dynsym[5] // 24

    total = 0
    near = []
    for i in range(n):
        o = dynsym[4] + i * 24
        nameoff, info, other, shndx, value, size = struct.unpack_from(
            "<IBBHQQ", blob, o)
        if value == 0:
            continue
        total += 1
        if lo <= value <= hi:
            e = blob.index(b"\x00", strtab + nameoff)
            near.append((value, blob[strtab + nameoff:e].decode()))

    print("dynsym entries      : %d" % n)
    print("nonzero dynsyms     : %d" % total)
    print("window              : 0x%x .. 0x%x" % (lo, hi))
    print("symbols in window   : %d" % len(near))
    for v, s in sorted(near):
        print("   0x%08x  %s" % (v, s))

    # Also: is the address inside an allocated section at all?
    for nameoff, st, fl, ad, of, sz in secs:
        if fl & 0x2 and ad <= lo < ad + sz:
            print("address lives in    : %s  (vaddr 0x%x size 0x%x)"
                  % (nm(nameoff), ad, sz))


if __name__ == "__main__":
    main(sys.argv[1], int(sys.argv[2], 0), int(sys.argv[3], 0))
