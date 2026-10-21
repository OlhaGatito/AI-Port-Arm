#!/usr/bin/env python3
"""Locate the managed function that raises a given string literal.

Why
---
Error 100 is not an exception (run25 proved that): the game evaluates its own
start-up self-check and then deliberately calls Utility.showDialog with the
text "An error has occurred.\\n100". The condition that failed therefore lives
in compiled managed code, and the only place left to read it is libil2cpp.so.

How IL2CPP makes that possible
------------------------------
In generated native code a C# string literal becomes

    MOV  W<n>, #<literalIndex>
    BL   il2cpp_string_literal_from_index

so the literal's index appears as a small immediate in a MOV. Searching the
text section for that immediate finds the instruction, and the surrounding
window (a few hundred bytes) is the function that uses it. Reading the other
immediates in that window lists the sibling literals the same code touches,
which is how the failing check names itself.

Usage:
  lit_find.py bbmeta/libil2cpp.so 6026
  lit_find.py bbmeta/libil2cpp.so 4390 4391 6026
"""
import argparse
import struct
import sys


def load_sections(path):
    """Return [(name, vaddr, file_offset, size, flags)] for allocated sections."""
    with open(path, "rb") as fh:
        blob = fh.read()
    if blob[:4] != b"\x7fELF":
        raise SystemExit("not an ELF file")
    is64 = blob[4] == 2
    if not is64:
        raise SystemExit("expected a 64-bit ELF")
    e_shoff, = struct.unpack_from("<Q", blob, 0x28)
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from("<HHH", blob, 0x3A)

    raw = []
    for i in range(e_shnum):
        off = e_shoff + i * e_shentsize
        name, stype, flags, addr, offset, size = struct.unpack_from(
            "<IIQQQQ", blob, off)
        raw.append((name, stype, flags, addr, offset, size))

    _, _, _, _, str_off, str_size = raw[e_shstrndx]
    strtab = blob[str_off:str_off + str_size]

    out = []
    for name, stype, flags, addr, offset, size in raw:
        end = strtab.find(b"\x00", name)
        sname = strtab[name:end].decode("ascii", "replace")
        # SHT_PROGBITS (1) with SHF_ALLOC|SHF_EXECINSTR. This NDK build puts
        # every managed method in a dedicated "il2cpp" section next to .text,
        # so both must be scanned or a literal index will appear to be absent.
        if stype == 1 and (flags & 0x4):
            out.append((sname, addr, offset, size, flags))
    return blob, out


def find_imm(blob, sections, value, limit=64):
    """Find MOV-immediate instructions carrying `value`, inside .text only.

    AArch64 has no 32-bit immediate MOV encoding, so what we actually search
    for is the MOVZ bit pattern: sf|opc=0b10|hw|imm16|Rd, i.e. the two words
    0x52800000 (32-bit) and 0xD2800000 (64-bit) with their sf/opc/hw/Rd bits
    masked off. Each hit is reported with the decoded shift so a coincidental
    match on a large constant is easy to tell apart from a literal index.
    """
    hits = []
    for name, base, offset, size, _ in sections:
        body = blob[offset:offset + size]
        for i in range(0, len(body) - 4, 4):
            insn, = struct.unpack_from("<I", body, i)
            if (insn & 0x7F800000) not in (0x52800000, 0xD2800000):
                continue
            hw = (insn >> 21) & 0x3
            imm16 = (insn >> 5) & 0xFFFF
            if (imm16 << (hw * 16)) == value:
                rd = insn & 0x1F
                hits.append((name, base, base + i, "movz", rd, hw * 16))
                if len(hits) >= limit:
                    return hits
    return hits


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("so", help="path to libil2cpp.so")
    ap.add_argument("indices", nargs="+", type=int,
                    help="string literal indices to locate")
    ap.add_argument("--window", type=int, default=256,
                    help="bytes of context to print around each hit")
    args = ap.parse_args()

    blob, sections = load_sections(args.so)
    print("text sections: " + ", ".join(
        "%s@0x%X+0x%X" % (n, a, s) for n, a, _, s, _ in sections), flush=True)

    for value in args.indices:
        hits = find_imm(blob, sections, value)
        print("\n=== literal index %d (0x%X): %d MOV site(s) ==="
              % (value, value, len(hits)), flush=True)
        for sec, base, vaddr, kind, rd, shift in hits:
            print("  %s+0x%X  vaddr 0x%X  %s w%d, #%d << %d"
                  % (sec, vaddr - base, vaddr, kind, rd, value, shift),
                  flush=True)
            start = max(0, vaddr - base - args.window)
            end = min(len(blob), vaddr - base + args.window)
            print("    --- context 0x%X..0x%X ---" % (vaddr - base - (vaddr - base - start),
                                                     vaddr - base + (end - (vaddr - base))))
            for off in range(start, end, 4):
                insn, = struct.unpack_from("<I", blob, off)
                mark = ">>" if off == vaddr - base else "  "
                note = ""
                if (insn & 0x7F800000) in (0x52800000, 0xD2800000):
                    hw = (insn >> 21) & 0x3
                    imm16 = (insn >> 5) & 0xFFFF
                    note = "  ; movz #%d" % (imm16 << (hw * 16))
                elif (insn & 0xFC000000) == 0x94000000:
                    note = "  ; bl"
                print("    %s 0x%08X%s" % (mark, insn, note))

    return 0


if __name__ == "__main__":
    sys.exit(main())
