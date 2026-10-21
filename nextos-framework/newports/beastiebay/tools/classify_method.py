#!/usr/bin/env python3
"""Identify the generated method that contains a given code offset.

run39h measured where the error-100 exception is thrown: the return address
of the intercepted `bl 0x600888` is 0xa4a674, and the code right before it is
the textbook lazy-init shape

    ldr  x20, [x19, #0x18]
    cbnz x20, #skip
    mov  x0, xzr
    bl   #0x600888        <- throw helper
  skip:

The binary is stripped, so the name is not sitting next to the code.  Two
things narrow it down without a debugger:

1. The class pointer is loaded from a data slot (e.g. `adrp x8, #0x1644000`
   + `ldr x8, [x8, #0xf60]`).  .rela.dyn holds a relocation at that slot, and
   IL2CPP's exported symbols are named after the type, so the slot's symbol
   often names the class outright.

2. Walking backwards for the function entry (the `stp x29, x30` prologue)
   gives the method start; the constants referenced from its first block are
   the method's own metadata (field names, class literals).

Usage:
  classify_method.py libil2cpp.so 0xa4a674 [--back 0x800]
"""
import argparse
import re
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
    stroff = out[e_shstrndx][4]

    def nm(o):
        e = blob.index(b"\x00", stroff + o)
        return blob[stroff + o:e].decode()
    return [(nm(s[0]),) + s[1:] for s in out]


def dyn_rela(blob, secs):
    """{r_offset: (symbol_name_or_None, addend)} for .rela.dyn."""
    dynsym = next(s for s in secs if s[0] == ".dynsym")
    dynstr = next(s for s in secs if s[0] == ".dynstr")
    rela = next(s for s in secs if s[0] == ".rela.dyn")

    def nm(o):
        e = blob.index(b"\x00", dynstr[4] + o)
        return blob[dynstr[4] + o:e].decode()

    out = {}
    n = rela[5] // 24
    for i in range(n):
        r_off, r_info, r_add = struct.unpack_from(
            "<QQq", blob, rela[4] + i * 24)
        sym = r_info >> 32
        name = nm(struct.unpack_from(
            "<I", blob, dynsym[4] + sym * 24)[0]) if sym else None
        out[r_off] = (name, r_add)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("so")
    ap.add_argument("addr", type=lambda s: int(s, 0))
    ap.add_argument("--back", type=lambda s: int(s, 0), default=0x800)
    args = ap.parse_args()

    blob = open(args.so, "rb").read()
    secs = sections(blob)
    rela = dyn_rela(blob, secs)
    target = args.addr

    # 1. Which data slots does the window load class pointers from?
    #    adrp xN, #imm  ->  ldr xN, [xN, #imm2]  ->  slot = imm + imm2
    print("== class-ish data slots referenced near 0x%x ==" % target)
    lo = max(0, target - args.back)
    hi = min(len(blob), target + 0x40)
    win = blob[lo:hi]
    for m in re.finditer(b"\x90\x00\x00", win):  # placeholder, real pass below
        pass

    # Do it properly with the disassembler rather than byte patterns.
    try:
        import capstone
    except ImportError:
        print("capstone missing; pip install capstone", file=sys.stderr)
        return 1
    md = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_ARM)
    md.detail = True

    slots = []
    insns = list(md.disasm(win, lo))
    for i, ins in enumerate(insns):
        if ins.mnemonic != "adrp":
            continue
        page = ins.operands[1].imm
        for j in range(i + 1, min(i + 4, len(insns))):
            nxt = insns[j]
            if nxt.mnemonic != "ldr" or not nxt.op_str.startswith("x"):
                break
            m2 = re.match(r"x\d+, \[x\d+, #(0x[0-9a-f]+)\]", nxt.op_str)
            if not m2:
                break
            slots.append(page + int(m2.group(1), 16))

    seen = set()
    for s in slots:
        if s in seen:
            continue
        seen.add(s)
        hit = rela.get(s)
        print("  slot 0x%09x -> %s" % (s, hit if hit else "(no relocation)"))

    # 2. Function entry: nearest stp x29, x30 backwards.
    print()
    print("== nearest function entry backwards ==")
    entry = None
    for ins in reversed(insns):
        if ins.mnemonic == "stp" and "x29, x30" in ins.op_str:
            entry = ins.address
            break
    if entry is None:
        print("  prologue not found within 0x%x" % args.back)
    else:
        print("  entry 0x%x  (0x%x bytes before the throw site)"
              % (entry, target - entry))
        print("  first instructions:")
        for ins in insns:
            if ins.address < entry or ins.address >= entry + 0x40:
                continue
            print("    0x%x  %-8s %s" % (ins.address, ins.mnemonic, ins.op_str))
    return 0


if __name__ == "__main__":
    sys.exit(main())
