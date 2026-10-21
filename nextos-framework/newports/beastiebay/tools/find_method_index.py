#!/usr/bin/env python3
"""Find which IL2CPP method implementation lives at a given code address.

run39h measured the throw site of the error-100 exception: 0xa4a59c is the
function entry, and inside it `ldr x20, [x19, #0x18]` / `cbnz` / `bl
0x600888` raises NullReferenceException when this+0x18 is NULL.  Knowing
WHICH method that is decides the next step, and the binary is stripped so
the name is not next to the code.

IL2CPP keeps a flat `s_Il2CppMethodPointers` table in .data.rel.ro, indexed
by MethodDefinition index -- the very same index global-metadata.dat uses.
The table is reached through .rela.dyn RELATIVE relocations whose addend is
the code address, so the offset of the entry inside the table IS the method
index (once the table base is known).

This script does the ELF half of that: locate the pointer table, find the
entry equal to the address, and report its index.  Turning the index into a
type+name needs the metadata half, which meta_dump.py covers.

Usage:
  find_method_index.py libil2cpp.so 0xa4a59c
"""
import struct
import sys


def sections(blob):
    e_shoff, = struct.unpack_from("<Q", blob, 0x28)
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from("<HHH", blob, 0x3A)
    raw = []
    for i in range(e_shnum):
        off = e_shoff + i * e_shentsize
        raw.append(struct.unpack_from("<IIQQQQ", blob, off))
    stroff = raw[e_shstrndx][4]

    def nm(o):
        e = blob.index(b"\x00", stroff + o)
        return blob[stroff + o:e].decode()
    return [(nm(r[0]),) + r[1:] for r in raw]


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    so = sys.argv[1]
    target = int(sys.argv[2], 0)
    blob = open(so, "rb").read()
    secs = sections(blob)

    rela = next(s for s in secs if s[0] == ".rela.dyn")
    dro = next(s for s in secs if s[0] == ".data.rel.ro")
    dro_lo, dro_hi = dro[3], dro[3] + dro[5]

    # Collect RELATIVE relocations that point at code, in table order.
    # A code address is inside an EXECINSTR section; data pointers are not.
    # NOTE the tuple order: (name, type, flags, addr, offset, size).
    # sh_flags is index 2, NOT index 4 (that one is sh_offset).
    exec_ranges = [(s[3], s[3] + s[5]) for s in secs if s[2] & 0x4]

    def is_code(a):
        return any(lo <= a < hi for lo, hi in exec_ranges)

    entries = []
    n = rela[5] // 24
    for i in range(n):
        r_off, r_info, r_add = struct.unpack_from(
            "<QQq", blob, rela[4] + i * 24)
        # R_AARCH64_RELATIVE is type 1027 and its low 32 bits are 0x403 --
        # NOT zero.  Testing "low 32 bits nonzero" for "has a symbol" skips
        # exactly the entries wanted here; compare the type itself.
        if (r_info & 0xffffffff) != 1027:  # R_AARCH64_RELATIVE
            continue
        if not is_code(r_add):
            continue
        if not (dro_lo <= r_off < dro_hi):
            continue
        entries.append((r_off, r_add))
    entries.sort()

    print("code pointers in .data.rel.ro : %d" % len(entries))
    print("table span                    : 0x%x .. 0x%x"
          % (entries[0][0], entries[-1][0]))

    for i, (off, val) in enumerate(entries):
        if val == target:
            print()
            print("FOUND 0x%x at slot %d (offset 0x%x)" % (target, i, off))
            base = entries[0][0]
            print("table base 0x%x -> raw index %d" % (base, (off - base) // 8))
            lo = max(0, i - 4)
            hi = min(len(entries), i + 5)
            print("neighbours:")
            for j in range(lo, hi):
                mark = ">>" if j == i else "  "
                print("%s slot %-7d 0x%x -> 0x%x"
                      % (mark, j, entries[j][0], entries[j][1]))
            return 0

    print("not present -- the address may be reached by a direct branch only")
    print()
    print("Interpretation.  A MISS is evidence, not a failure.  Every address")
    print("the reflection layer can name is in this table, so a miss means the")
    print("code is NOT a managed method -- in this binary, that is the IL2CPP")
    print("runtime itself.  Cross-check with a known-good address before")
    print("trusting a miss:")
    print()
    print("  find_method_index.py libil2cpp.so 0x897068   # BootForm::Init")
    print()
    print("That one hits, and its neighbours are the BootForm methods the")
    print("runtime log already printed -- so the table and this script are")
    print("both working, which is what makes the negative result meaningful.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
