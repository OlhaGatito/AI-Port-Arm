#!/usr/bin/env python3
"""Find where IL2CPP-generated code references a managed string literal.

Why this shape of search
------------------------
The usual advice for IL2CPP literal hunting ("look for MOVZ #index") does not
match this binary: the shipping libil2cpp.so is stripped of its dynamic symbols
(it carries a .gnu_debuglink to a separate debug file), and the codegen helpers
that would name the reference are absent from .dynstr entirely. So the index
cannot be named by symbol, only recognised structurally.

What is still reliable is the index itself. The metadata stringLiteral table
has 9311 entries, and a C# literal becomes a call that passes its index to the
literal-from-index helper. In practice the index shows up either as

  * a 32-bit MOVZ immediate               (MOV W0, #0x178A)
  * a MOVK/MOVZ pair building a 64-bit    (rare; index is always < 2^16 here)
  * a 32-bit literal pool entry consumed by LDR (Unity's codegen emits a
    per-function constant pool, so this is the common case)

so all three are scanned and each hit is reported with its decoded instruction
and the surrounding pool, which is what names the sibling literals.

Usage:
  lit_refs.py bbmeta/libil2cpp.so 6026            # literals near "An error has occurred.\n100"
  lit_refs.py bbmeta/libil2cpp.so 6026 --pool 6
"""
import argparse
import struct
import sys

try:
    from capstone import Cs, CS_ARCH_ARM64, CS_MODE_ARM
except ImportError:
    raise SystemExit("capstone is required: pip install capstone")


def load_text_sections(blob):
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
        if stype == 1 and (flags & 0x4):
            out.append((sname, addr, offset, size))
    return out


def scan_section(md, blob, sname, base, offset, size, targets, pool):
    hits = []
    body = blob[offset:offset + size]
    for i in range(0, len(body) - 4, 4):
        insn, = struct.unpack_from("<I", body, i)
        value = None
        # MOVZ 32/64-bit: sf|opc=10 | hw | imm16 | Rd
        if (insn & 0x7F800000) in (0x52800000, 0xD2800000):
            hw = (insn >> 21) & 0x3
            imm16 = (insn >> 5) & 0xFFFF
            value = imm16 << (hw * 16)
        if value is None or value not in targets:
            continue
        # Decode a small window so the report is readable, and pull any nearby
        # 32-bit constants that plausibly index the same pool.
        start = max(0, i - 16)
        end = min(len(body), i + 20)
        window = body[start:end]
        decoded = []
        for (addr, size_i, mnemonic, op_str) in md.disasm(window, base + start):
            decoded.append("      0x%X  %-8s %s" % (addr, mnemonic, op_str))
        near = []
        for j in range(max(0, i - 64), min(len(body) - 4, i + 68), 4):
            word, = struct.unpack_from("<I", body, j)
            if word in targets:
                near.append((base + j, word))
        hits.append({
            "section": sname, "vaddr": base + i, "value": value,
            "insn": insn, "decoded": decoded, "near": near, "pool": pool,
        })
    return hits


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("so")
    ap.add_argument("indices", nargs="+", type=int)
    ap.add_argument("--pool", type=int, default=4,
                    help="how many nearby same-set literal indices to list")
    args = ap.parse_args()

    blob = open(args.so, "rb").read()
    sections = load_text_sections(blob)
    md = Cs(CS_ARCH_ARM64, CS_MODE_ARM)
    md.detail = False
    targets = set(args.indices)

    print("scanning: " + ", ".join(
        "%s@0x%X+0x%X" % (name, base, size) for name, base, _, size in sections),
        flush=True)
    total = 0
    for sname, base, offset, size in sections:
        hits = scan_section(md, blob, sname, base, offset, size, targets, args.pool)
        for h in hits:
            total += 1
            print("\n=== literal %d (0x%X) at %s+0x%X (vaddr 0x%X) ==="
                  % (h["value"], h["value"], h["section"],
                     h["vaddr"] - base, h["vaddr"]), flush=True)
            for line in h["decoded"]:
                print(line, flush=True)
            if h["near"]:
                print("      other target indices nearby: %s"
                      % ", ".join("0x%X@0x%X" % (w, a) for a, w in h["near"]),
                      flush=True)
    print("\n--- %d hit(s) ---" % total, flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
