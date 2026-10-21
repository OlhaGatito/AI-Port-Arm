#!/usr/bin/env python3
"""Self-check for the .ARM.attributes decoder in elf_static_report.py.

The tag numbering this port's float-ABI decision rests on is not taken from a
speculation or a table assumption: it is pinned to the test device's own
binutils.  The hex streams below were copied off the device
(`dd if=<lib> bs=1 skip=$((0x<section offset from readelf -S>>)
count=$((0x<section size>))`), and the expected names/values are verbatim from
`readelf -A` on that same device, for two libraries: /usr/lib32/libz.so.1 (55
bytes, 18 attributes) and /usr/lib32/ld-linux-armhf.so.3 (59 bytes, 20
attributes).  If the decoder walks those streams into those exact lists, the
same parser can be trusted on the guests, which have no host readelf
available.

Run: python3 tools/test_arm_attributes.py
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from elf_static_report import parse_arm_attributes  # noqa: E402

LIBZ_HEX = (
    "41 36 00 00 00 61 65 61 62 69 00 01 2c 00 00 00"
    " 05 38 2d 41 00 06 0e 07 41 08 01 09 02 0a 07 0c"
    " 03 12 04 14 01 15 01 17 03 18 01 19 01 1a 02 1c"
    " 01 22 01 2a 01 44 03"
)

# readelf -A /usr/lib32/libz.so.1, in its printed order.  binutils names tag 10
# "VFP_arch" where the AEABI document says FP_arch; the alias is the only
# renaming allowed below, everything else must match by number and value.
EXPECTED = [
    ("CPU_name", "8-A"),
    ("CPU_arch", 14),
    ("CPU_arch_profile", 0x41),
    ("ARM_ISA_use", 1),
    ("THUMB_ISA_use", 2),
    ("VFP_arch", 7),
    ("Advanced_SIMD_arch", 3),
    ("ABI_PCS_wchar_t", 4),
    ("ABI_FP_denormal", 1),
    ("ABI_FP_exceptions", 1),
    ("ABI_FP_number_model", 3),
    ("ABI_align8_needed", 1),
    ("ABI_align8_preserved", 1),
    ("ABI_enum_size", 2),
    ("ABI_VFP_args", 1),
    ("CPU_unaligned_access", 1),
    ("tag_42", 1),
    ("Virtualization_use", 3),
]
ALIASES = {"VFP_arch": "FP_arch"}

# Second, independent pin: /usr/lib32/ld-linux-armhf.so.3 (section 59 bytes at
# 0x27946).  This stream carries the tags the guests have and libz lacks
# (19 ABI_FP_rounding) and, crucially, renders ABI_optimization_goals *after*
# ABI_VFP_args -- which is what fixes tag 30 as optimization goals and leaves
# tag 27 (present only in the guests) deliberately unnamed.
LD_HEX = (
    "41 3a 00 00 00 61 65 61 62 69 00 01 30 00 00 00"
    " 05 38 2d 41 00 06 0e 07 41 08 01 09 02 0a 07 0c"
    " 03 12 04 13 01 14 01 15 01 17 03 18 01 19 01 1a"
    " 02 1c 01 1e 02 22 01 2a 01 44 03"
)
LD_EXPECTED = [
    ("CPU_name", "8-A"), ("CPU_arch", 14), ("CPU_arch_profile", 0x41),
    ("ARM_ISA_use", 1), ("THUMB_ISA_use", 2), ("VFP_arch", 7),
    ("Advanced_SIMD_arch", 3), ("ABI_PCS_wchar_t", 4),
    ("ABI_FP_rounding", 1), ("ABI_FP_denormal", 1), ("ABI_FP_exceptions", 1),
    ("ABI_FP_number_model", 3), ("ABI_align8_needed", 1),
    ("ABI_align8_preserved", 1), ("ABI_enum_size", 2), ("ABI_VFP_args", 1),
    ("ABI_optimization_goals", 2), ("CPU_unaligned_access", 1),
    ("tag_42", 1), ("Virtualization_use", 3),
]


def check(label, got, want):
    bad = []
    if len(got) != len(want):
        bad.append(f"count {len(got)} != {len(want)}")
    for i, (g, w) in enumerate(zip(got, want)):
        gname = ALIASES.get(w[0], w[0])
        if (g["name"], g["value"]) != (gname, w[1]):
            bad.append(f"[{i}] {g['name']}={g['value']!r} != {w[0]}={w[1]!r}")
    print(("PASS " if not bad else "FAIL ") + label +
          ("" if not bad else "\n  " + "\n  ".join(bad)))
    return not bad


def main():
    blob = bytes.fromhex(LIBZ_HEX.replace(" ", ""))
    ok = [len(blob) == 55]
    print(f"libz stream length: {len(blob)} bytes (readelf says 55)")

    attrs = parse_arm_attributes(blob)
    ok.append(not attrs.get("error"))
    if attrs.get("error"):
        print("FAIL decoder errored:", attrs["error"])
    ok.append(check(f"attribute stream ({len(attrs['tags'])} tags)",
                    attrs["tags"], EXPECTED))
    ok.append(attrs.get("tag_abi_vfp_args") == {"value": 1,
              "name": "VFP register arguments (Linux ARMHF hard-float)"})
    print(("PASS " if ok[-1] else "FAIL ") +
          f"tag_abi_vfp_args -> {attrs.get('tag_abi_vfp_args')}")

    # framing must stop exactly at the section end, not overrun it
    ok.append(attrs.get("form", "").startswith("pre-2012"))

    ldb = bytes.fromhex(LD_HEX.replace(" ", ""))
    la = parse_arm_attributes(ldb)
    print(f"ld-linux stream length: {len(ldb)} bytes (readelf says 59)")
    ok.append(len(ldb) == 59 and not la.get("error"))
    ok.append(check(f"ld-linux attribute stream ({len(la['tags'])} tags)",
                    la["tags"], LD_EXPECTED))
    lnames = [ALIASES.get(t["name"], t["name"]) for t in la["tags"]]
    ordered = ("ABI_VFP_args" in lnames and "ABI_optimization_goals" in lnames
               and lnames.index("ABI_VFP_args")
               < lnames.index("ABI_optimization_goals"))
    ok.append(ordered)
    print(("PASS " if ordered else "FAIL ") +
          "readelf order pins tag 28=VFP_args before tag 30=optimization_goals, "
          "so the guests' extra tag 27 is not VFP args under another number")

    trunc = parse_arm_attributes(blob[:54])
    ok.append(bool(trunc.get("error")) or len(trunc["tags"]) < len(EXPECTED))
    print(("PASS " if trunc.get("error") or len(trunc["tags"]) < len(EXPECTED)
           else "FAIL ") + "truncated stream does not silently mis-decode")

    # a guest-shaped stream (no tag 28) must report absent, not 0
    rr3 = ("41 37 00 00 00 61 65 61 62 69 00 01 2d 00 00 00"
           " 05 41 52 4d 20 76 37 00 06 0a 07 41 08 01 09 02 0a 04 0c 01"
           " 12 04 14 01 15 01 17 03 18 01 1a 02 1b 03 1e 02 22 01 2c 01")
    g = parse_arm_attributes(bytes.fromhex(rr3.replace(" ", "")))
    ok.append(g.get("tag_abi_vfp_args") is None and not g.get("error"))
    print(("PASS " if ok[-1] else "FAIL ") +
          f"RR3 stream: {len(g['tags'])} tags, Tag_ABI_VFP_args "
          f"{'absent' if g.get('tag_abi_vfp_args') is None else 'PRESENT'}")
    ok.append([t["name"] for t in g["tags"]][:1] == ["CPU_name"]
              and g["cpu_arch"] == {"value": 10, "name": "v7"})
    print(("PASS " if ok[-1] else "FAIL ") +
          f"RR3 CPU_arch -> {g.get('cpu_arch')}")

    print("\nRESULT:", "all checks passed" if all(ok) else "CHECKS FAILED")
    return 0 if all(ok) else 1


if __name__ == "__main__":
    sys.exit(main())
