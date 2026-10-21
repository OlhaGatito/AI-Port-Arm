#!/usr/bin/env python3
"""Search libil2cpp.so's managed-code section for a raw 32-bit constant.

Why a blunt byte search is the right tool here
---------------------------------------------
The obvious approach -- look for MOVZ #<literal index> -- returns nothing on
this binary, and that is a fact about the build rather than a failed search:

  * the shipping libil2cpp.so is stripped (.gnu_debuglink points at a separate
    debug file), so the codegen helpers that would name the reference
    (il2cpp_string_literal_from_index and friends) are absent from .dynstr;
  * Unity 2018's codegen materialises literals through a per-function
    constant pool reached by PC-relative LDR, so the index is not necessarily
    an immediate operand at all.

What survives all of that is the constant itself. Managed code that raises
"An error has occurred.\n100" has to materialise the value 100 somewhere --
as an immediate, in the function's literal pool, or as data. So this tool
searches the executable sections for the 4-byte little-endian encoding of a
value and reports each hit with its decoded instruction and surrounding
context, which is what identifies the function.

Usage:
  const_find.py bbmeta/libil2cpp.so 100
  const_find.py bbmeta/libil2cpp.so 100 --ctx 48 --limit 12
"""
import argparse
import struct
import sys

try:
    from capstone import Cs, CS_ARCH_ARM64, CS_MODE_ARM
except ImportError:
    raise SystemExit("capstone is required: pip install capstone")


def exec_sections(blob):
    e_shoff, = struct.unpack_from("<Q", blob, 0x28)
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from("<HHH", blob, 0x3A)
    str_off, = struct.unpack_from("<Q", blob, e_shoff + e_shstrndx * e_shentsize + 0x18)
    str_size, = struct.unpack_from("<Q", blob, e_shoff + e_shstrndx * e_shentsize + 0x20)
    strtab = blob[str_off:str_off + str_size]
    out = []
    for i in range(e_shnum):
        off = e_shoff + i * e_shentsize
        name, stype, flags, addr, offset, size = struct.unpack_from(
            "<IIQQQQ", blob, off)
        end = strtab.find(b"\x00", name)
        sname = strtab[name:end].decode("ascii", "replace")
        # Managed code lives in the dedicated "il2cpp" section on this NDK
        # build; .text holds the C/C++ runtime. Both are executable.
        if stype == 1 and (flags & 0x4):
            out.append((sname, addr, offset, size))
    return out


def describe(word):
    """Best-effort decode of an AArch64 word, for readability only."""
    if (word & 0x7F800000) in (0x52800000, 0xD2800000):
        hw = (word >> 21) & 0x3
        imm16 = (word >> 5) & 0xFFFF
        sf = "x" if word & 0x80000000 else "w"
        return "mov%s%d, #%d" % (sf, word & 0x1F, imm16 << (hw * 16))
    if (word & 0xFF000000) == 0x54000000:
        imm = (word >> 5) & 0x7FFFF
        if imm & 0x40000:
            imm -= 0x80000
        return "b #%d" % imm
    if (word & 0xFC000000) == 0x94000000:
        imm = word & 0x03FFFFFF
        if imm & 0x02000000:
            imm -= 0x04000000
        return "bl #%d" % imm
    if (word & 0x3B000000) == 0x39000000:
        return "ldr/str (imm)"
    if (word & 0x9B000000) == 0x90000000:
        return "adrp"
    return ""


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("so")
    ap.add_argument("value", type=lambda s: int(s, 0))
    ap.add_argument("--ctx", type=int, default=40,
                    help="bytes of context decoded around each hit")
    ap.add_argument("--limit", type=int, default=40)
    ap.add_argument("--section", default=None,
                    help="restrict to one section (e.g. il2cpp)")
    args = ap.parse_args()

    blob = open(args.so, "rb").read()
    sections = exec_sections(blob)
    if args.section:
        sections = [s for s in sections if s[0] == args.section]
        if not sections:
            raise SystemExit("no such section: %s" % args.section)
    md = Cs(CS_ARCH_ARM64, CS_MODE_ARM)

    needle = struct.pack("<I", args.value & 0xFFFFFFFF)
    print("searching for 0x%08X in: %s"
          % (args.value & 0xFFFFFFFF,
             ", ".join("%s+0x%X" % (n, sz) for n, _, _, sz in sections)),
          flush=True)

    total = 0
    for sname, base, offset, size in sections:
        body = blob[offset:offset + size]
        pos = 0
        while True:
            i = body.find(needle, pos)
            if i < 0:
                break
            pos = i + 1
            if i % 4 != 0:
                continue  # unaligned: cannot be an instruction or pool word
            total += 1
            word, = struct.unpack_from("<I", body, i)
            print("\n--- %s+0x%X (vaddr 0x%X) : 0x%08X  %s"
                  % (sname, i, base + i, word, describe(word)), flush=True)
            start = max(0, i - args.ctx)
            end = min(len(body), i + args.ctx)
            for (addr, size_i, mnem, ops) in md.disasm(body[start:end], base + start):
                mark = ">>" if addr == base + i else "  "
                print("    %s 0x%08X  %-8s %s" % (mark, addr, mnem, ops), flush=True)
            if total >= args.limit:
                print("\n--- limit %d reached ---" % args.limit, flush=True)
                return 0
    print("\n--- %d aligned hit(s) ---" % total, flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
