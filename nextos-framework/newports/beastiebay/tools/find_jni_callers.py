#!/usr/bin/env python3
"""Find the IL2CPP call sites that invoke one of OUR exported JNI thunks.

run32 lesson: backtrace() from inside the JNI handler cannot see the managed
caller.  IL2CPP reaches our handler through a function pointer it obtained
from JNIEnv, and the call is an ordinary `blr` -- so the return address *is*
on the stack, but only for the frame that made the call, and our dispatcher
(`bl`-heavy, no frame pointer to follow) erases the chain before we can walk
it.  Four frames of "our own code" is the same answer as zero frames.

The reliable direction is the other one: JNI thunks are looked up by name, so
the name string lives in libil2cpp.so's data and the call goes through a
register-loaded slot.  Rather than guess the register, scan the managed text
for BL/BLR instructions whose *target* is one of the fixed JNI bridges we
export -- the bridges have fixed addresses, so a call site is recognisable
without any symbol.

Usage:
  find_jni_callers.py libil2cpp.so --bridge 0x1c4c0 --bridge 0x1d2b0
"""
import argparse
import struct
import sys

try:
    import capstone
except ImportError:
    raise SystemExit("capstone is required: use the sshtool venv python")


def alloc_sections(blob):
    e_shoff, = struct.unpack_from("<Q", blob, 0x28)
    e_shentsize, e_shnum, _ = struct.unpack_from("<HHH", blob, 0x3A)
    out = []
    for i in range(e_shnum):
        off = e_shoff + i * e_shentsize
        _, sh_type, sh_flags = struct.unpack_from("<IIQ", blob, off)
        sh_addr, sh_offset, sh_size = struct.unpack_from("<QQQ", blob, off + 0x10)
        if (sh_type == 1 or sh_type == 8) and (sh_flags & 0x4):
            out.append((sh_addr, sh_offset, sh_size))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("so")
    ap.add_argument("--bridge", action="append", default=[],
                    help="file-relative vaddr of an exported JNI bridge (repeatable)")
    ap.add_argument("--limit", type=int, default=80)
    args = ap.parse_args()

    blob = open(args.so, "rb").read()
    wanted = {int(b, 0) for b in args.bridge}
    if not wanted:
        raise SystemExit("pass at least one --bridge")

    md = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_ARM)
    md.detail = False
    hits = []
    for addr, off, size in alloc_sections(blob):
        # 4 bytes per instruction; AArch64 has no variable length here.
        for i in range(0, size - 4, 4):
            here = addr + i
            if here in wanted:
                continue
            word, = struct.unpack_from("<I", blob, off + i)
            if (word & 0xFC000000) == 0x94000000:  # BL imm26
                imm = word & 0x03FFFFFF
                if imm & 0x02000000:
                    imm -= 0x04000000
                target = here + imm * 4
                if target in wanted:
                    hits.append((here, target))
            elif (word & 0xFFFFFC1F) == 0xD63F0000:  # BLR Xn
                hits.append((here, "blr x%d" % (word & 0x1F)))
    print("bl sites landing on a bridge: %d" % sum(1 for h in hits if isinstance(h[1], int)))
    for here, target in hits[:args.limit]:
        print("  0x%08x  ->  %s" % (here, ("0x%x" % target) if isinstance(target, int) else target))
    return 0


if __name__ == "__main__":
    sys.exit(main())
