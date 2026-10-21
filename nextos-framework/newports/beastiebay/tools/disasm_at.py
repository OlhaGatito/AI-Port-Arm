#!/usr/bin/env python3
"""Disassemble libil2cpp.so at a raw offset and print the surrounding basic blocks.

run32 turns the showDialog backtrace into something actionable.  Three of the
four frames used to print as "?+0x0" because fp2_mod_at only walked the game
objects, so the frame that would name the generated method had no offset to
look up.  With the offset in hand the question becomes concrete: which
comparison decides STATE_ERROR, and what does it compare against?

The binary is stripped, so there are no symbols to lean on.  What survives is
the per-function constant pool plus the PC-relative LDRs that read it, and
those are enough to recognise a call to Utility.showDialog and the branches
that guard it.

Usage:
  disasm_at.py libil2cpp.so 0x1234 [--before N] [--after N]
  disasm_at.py libil2cpp.so 0x1234 --calls      # only bl/bl* targets nearby
"""
import argparse
import struct
import sys

try:
    import capstone
except ImportError:
    raise SystemExit("capstone is required: use the sshtool venv python")


def sections(blob):
    """Return [(name, addr, offset, size, flags)] for allocated sections."""
    if blob[:4] != b"\x7fELF":
        raise SystemExit("not an ELF file")
    e_shoff, = struct.unpack_from("<Q", blob, 0x28)
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from("<HHH", blob, 0x3A)
    out = []
    for i in range(e_shnum):
        off = e_shoff + i * e_shentsize
        name_off, sh_type, sh_flags = struct.unpack_from("<IIQ", blob, off)
        sh_addr, sh_offset, sh_size = struct.unpack_from("<QQQ", blob, off + 0x10)
        if not (sh_flags & 0x2):  # SHF_ALLOC
            continue
        out.append((i, name_off, sh_type, sh_flags, sh_addr, sh_offset, sh_size))
    strtab_off = out[0][0]  # section 0 header, used to resolve names lazily
    hdr = e_shoff + strtab_off * e_shentsize
    str_off, = struct.unpack_from("<Q", blob, hdr + 0x18)
    named = []
    for (_, name_off, sh_type, sh_flags, sh_addr, sh_offset, sh_size) in out:
        end = blob.index(b"\x00", str_off + name_off)
        name = blob[str_off + name_off:end].decode("ascii", "replace")
        named.append((name, sh_addr, sh_offset, sh_size, sh_flags, sh_type))
    return named


def to_offset(secs, vaddr):
    for name, addr, off, size, flags, stype in secs:
        if addr <= vaddr < addr + size:
            return off + (vaddr - addr), name
    return None, None


def auto(value):
    """Offsets arrive as 0x... from the backtrace; argparse's int is decimal."""
    return int(value, 0)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("so")
    ap.add_argument("addr", help="file-relative virtual address, e.g. 0x1c95a4")
    ap.add_argument("--before", type=auto, default=0x40)
    ap.add_argument("--after", type=auto, default=0x100)
    ap.add_argument("--calls", action="store_true",
                    help="print only branch-with-link targets in the window")
    args = ap.parse_args()

    blob = open(args.so, "rb").read()
    secs = sections(blob)
    target = auto(args.addr)
    foff, sec = to_offset(secs, target)
    if foff is None:
        raise SystemExit("0x%x is not inside any allocated section" % target)
    start = max(0, foff - args.before)
    end = min(len(blob), foff + args.after)

    md = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_ARM)
    md.detail = False
    print("section %s  file offset 0x%x  target vaddr 0x%x" % (sec, foff, target))
    print("-" * 72)
    for ins in md.disasm(blob[start:end], target - args.before):
        mark = ">>" if ins.address == target else "  "
        text = "%s 0x%x  %-8s %s" % (mark, ins.address, ins.mnemonic, ins.op_str)
        if args.calls:
            if ins.mnemonic.startswith("bl"):
                print(text)
            continue
        print(text)
    print("-" * 72)
    return 0


if __name__ == "__main__":
    sys.exit(main())
