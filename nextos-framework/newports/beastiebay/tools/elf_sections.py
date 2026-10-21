#!/usr/bin/env python3
"""List the ELF sections of a shared object, so tooling can be pointed at the
right one. Android NDK builds put executable code in .text, but stripped
shipping libraries sometimes fold everything into a single PT_LOAD with
minimal section headers -- knowing which is in play avoids a tool that
silently searches zero bytes and reports "not found".
"""
import struct
import sys


def main():
    path = sys.argv[1]
    with open(path, "rb") as fh:
        blob = fh.read()
    if blob[:4] != b"\x7fELF":
        raise SystemExit("not an ELF file")
    e_shoff, = struct.unpack_from("<Q", blob, 0x28)
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from("<HHH", blob, 0x3A)
    print("size %d  shoff 0x%X  entsize %d  num %d  strndx %d"
          % (len(blob), e_shoff, e_shentsize, e_shnum, e_shstrndx))

    str_off, = struct.unpack_from("<Q", blob, e_shoff + e_shstrndx * e_shentsize + 0x18)
    str_size, = struct.unpack_from("<Q", blob, e_shoff + e_shstrndx * e_shentsize + 0x20)
    strtab = blob[str_off:str_off + str_size]

    alloc_exec = 0
    for i in range(e_shnum):
        off = e_shoff + i * e_shentsize
        name, stype, flags, addr, offset, size = struct.unpack_from(
            "<IIQQQQ", blob, off)
        end = strtab.find(b"\x00", name)
        sname = strtab[name:end].decode("ascii", "replace")
        tag = ""
        if (flags & 0x2) and (flags & 0x4):
            alloc_exec += size
            tag = "  <-- ALLOC|EXECINSTR"
        print("%-22s type=%-4d flags=0x%-5x addr=0x%09X off=0x%08X size=0x%08X%s"
              % (sname, stype, flags, addr, offset, size, tag))
    print("total ALLOC|EXECINSTR: 0x%X (%d bytes)" % (alloc_exec, alloc_exec))


if __name__ == "__main__":
    main()
