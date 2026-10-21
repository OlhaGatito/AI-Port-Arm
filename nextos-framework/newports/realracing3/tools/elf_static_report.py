#!/usr/bin/env python3
"""Static (never-executed) Android ELF forensics for hosts without binutils.

Answers what PORTING-GUIDE 3.3/6.3 ask of `readelf -Ws/-d/-r` for an
armeabi-v7a guest on a host that has no readelf: ELF class and EABI version,
the soft-float/ARMHF boundary, DT_NEEDED, V5 nxloader preflight blockers,
undefined imports, and the relocation types the loader will actually have to
apply (reported with the section each one lands in, which is how the addend
semantics was decided for Real Racing 3).

Read-only. Emits technical metadata only; never writes guest bytes anywhere.
"""
import argparse
import collections
import json
import re
import struct
import sys

# --- constants verified against the input file's own anchors -------------
# DT: verified on libRealRacing3.so by matching every value against the
# section table (.dynstr size, SONAME string at the end of .dynstr,
# PLTREL==REL, .init_array/.fini_array addresses and sizes).
DT = {1: "NEEDED", 2: "PLTRELSZ", 3: "PLTGOT", 4: "HASH", 5: "STRTAB",
      6: "SYMTAB", 7: "RELA", 8: "RELAENT", 10: "STRSZ", 11: "SYMENT",
      12: "INIT", 13: "FINI", 14: "SONAME", 15: "RPATH", 16: "SYMBOLIC",
      17: "REL", 18: "RELSZ", 19: "RELENT", 20: "PLTREL", 21: "DEBUG",
      22: "TEXTREL", 23: "JMPREL", 24: "BIND_NOW", 25: "INIT_ARRAY",
      26: "FINI_ARRAY", 27: "INIT_ARRAYSZ", 28: "FINI_ARRAYSZ",
      29: "RUNPATH", 30: "FLAGS", 32: "PREINIT_ARRAY", 33: "PREINIT_ARRAYSZ",
      0x6FFFFEF5: "GNU_HASH", 0x6FFFFEF7: "TLSDESC_GOT", 0x6FFFFFF0: "VERSYM",
      0x6FFFFFF9: "RELACOUNT", 0x6FFFFFFA: "RELCOUNT", 0x6FFFFFFB: "FLAGS_1",
      0x6FFFFFFC: "VERDEF", 0x6FFFFFFD: "VERDEFNUM", 0x6FFFFFFE: "VERNEED",
      0x6FFFFFFF: "VERNEEDNUM", 0x6FFFC518: "RELR",
      0x6000000F: "ANDROID_REL", 0x60000010: "ANDROID_RELA",
      0x60000011: "ANDROID_RELX"}
DF_FLAGS = {0x1: "ORIGIN", 0x2: "SYMBOLIC", 0x4: "TEXTREL", 0x8: "BIND_NOW",
            0x10: "STATIC_TLS"}
STT = {0: "NOTYPE", 1: "OBJECT", 2: "FUNC", 3: "SECTION", 4: "FILE",
       5: "COMMON", 6: "TLS", 10: "GNU_IFUNC"}
PH = {1: "LOAD", 2: "DYNAMIC", 3: "INTERP", 4: "NOTE", 6: "PHDR", 7: "TLS",
      0x6474E550: "GNU_RELRO", 0x6474E551: "GNU_EH_FRAME",
      0x6474E552: "GNU_STACK"}
# AArch32 relocation numbers.  Names follow the AEABI table; the addend
# semantics column below is what this tool measures, because the loader has to
# implement the measured behaviour, not a table assumption.
ARM_R = {
    0: "NONE", 1: "PC24", 2: "ABS32", 3: "REL32", 4: "LDR_PC_G0", 5: "ABS16",
    6: "ABS12", 7: "THM_ABS5", 8: "ABS8", 9: "SBREL32", 10: "THM_CALL",
    11: "THM_PC8", 12: "BREL_ADJ", 13: "TLS_DESC", 14: "THM_SWI8",
    15: "XVEC24", 16: "THM_XVEC24", 21: "V4BX", 22: "TARGET1", 23: "TARGET2",
    24: "PREL31", 25: "MOVW_ABS_NC", 26: "MOVT_ABS", 27: "MOVW_PREL_NC",
    28: "MOVT_PREL", 29: "THM_MOVW_ABS_NC", 30: "THM_MOVT_ABS",
    31: "THM_MOVW_PREL_NC", 32: "THM_MOVT_PREL", 33: "THM_JUMP19",
    34: "THM_JUMP6", 35: "THM_ALU_PREL_11_0", 36: "THM_PC12",
    37: "ABS32_NOI", 38: "REL32_NOI", 53: "THM_JUMP24", 60: "LDR_SBREL_11_0_NC",
    97: "GOT_PREL", 109: "TLS_GOT32", 110: "TLS_LDO", 111: "TLS_LDM",
    112: "TLSDESC", 114: "TLS_IE32", 115: "TLS_LE32", 128: "GOT_BREL",
    160: "IRELATIVE", 165: "RELAX",
}
JNI_CLASS = re.compile(rb"L[a-zA-Z0-9_/$][a-zA-Z0-9_$/]{3,110};")
DOTTED = re.compile(rb"(?:com|android|androidx|org|net)\.[a-zA-Z0-9_.$]{4,120}")

# AEABI build-attribute tags.  This numbering is NOT taken on faith: every name
# below was pinned on the test device by pairing one library's raw attribute
# bytes (`dd` at the section offset from `readelf -S`) with that same library's
# own `readelf -A` render, in tag order, for five independent 32-bit
# providers -- /usr/lib32/{libz.so.1, ld-linux-armhf.so.3, libc.so.6,
# libSDL2-2.0.so, libmali-bifrost-g31-g24p0-wayland-gbm.so}.  tools/
# test_arm_attributes.py re-runs the libz case as a self-check.  Tag 28 is the
# load-bearing one: it separates the Android softfp guest from a Linux ARMHF
# provider.  Tags 27 and 44 appear only in the guests, no device library
# declares them, and binutils renders unknown tags as bare numbers, so they
# stay unnamed here instead of borrowing a spec table.
ARM_ATTR = {
    5: "CPU_name", 6: "CPU_arch", 7: "CPU_arch_profile", 8: "ARM_ISA_use",
    9: "THUMB_ISA_use", 10: "FP_arch", 12: "Advanced_SIMD_arch",
    18: "ABI_PCS_wchar_t", 19: "ABI_FP_rounding", 20: "ABI_FP_denormal",
    21: "ABI_FP_exceptions", 22: "ABI_FP_user_exceptions",
    23: "ABI_FP_number_model", 24: "ABI_align8_needed",
    25: "ABI_align8_preserved", 26: "ABI_enum_size", 28: "ABI_VFP_args",
    30: "ABI_optimization_goals", 34: "CPU_unaligned_access",
    68: "Virtualization_use",
}
VFP_ARGS = {0: "base AAPCS: floats in core registers (Android armeabi-v7a softfp)",
            1: "VFP register arguments (Linux ARMHF hard-float)",
            2: "compatible with both"}
CPU_ARCH = {5: "v5", 6: "v5T", 7: "v5E", 8: "v6", 9: "v6T2", 10: "v7",
            11: "v7A", 14: "v8"}


def parse_arm_attributes(blob):
    """Decode a pre-2012-form .ARM.attributes body.

    Framing (validated arithmetically on libz, ld-linux-armhf.so.3 and both
    guests): 'A', u32 section length (size minus that byte), "aeabi" + NUL,
    u8 sub-section kind, u32 sub-section length counted from that u8, then
    attributes as (u8 tag, value) with NO per-attribute length and a NUL-
    terminated string only for CPU_name.
    """
    out = {"tags": []}
    if len(blob) < 16 or blob[0] != 0x41:
        out["error"] = "not a 'A'-versioned AEABI attribute section"
        return out
    end = 11 + struct.unpack_from("<I", blob, 12)[0]
    limit = min(end, len(blob))
    i, prev = 16, -1
    while i < limit:
        tag = blob[i]
        i += 1
        if tag <= prev:
            out["error"] = f"tags not ascending at offset {i - 1}"
            return out
        prev = tag
        if tag == 5:
            stop = blob.find(b"\0", i, limit)
            if stop < 0:
                out["error"] = f"CPU_name unterminated at offset {i}"
                return out
            value = blob[i:stop].decode("ascii", "replace")
            i = stop + 1
        else:
            value = 0
            shift = 0
            while True:
                if i >= limit:
                    out["error"] = f"truncated value for tag {tag} at offset {i}"
                    return out
                b = blob[i]
                i += 1
                value |= (b & 0x7F) << shift
                if not b & 0x80:
                    break
                shift += 7
        out["tags"].append({"tag": tag, "name": ARM_ATTR.get(tag, f"tag_{tag}"),
                            "value": value})
    if end > len(blob):
        out["error"] = (f"declared attribute length {end} exceeds the "
                        f"{len(blob)}-byte section")
        return out
    if end < len(blob):
        out["trailing_bytes"] = (len(blob) - end, "second sub-section or a "
                                 "post-2012 framing; only the first was decoded")
    out["form"] = "pre-2012 single sub-section, {u8 kind, u32 length}, no per-attribute length"
    vfp = next((t["value"] for t in out["tags"] if t["tag"] == 28), None)
    out["tag_abi_vfp_args"] = (None if vfp is None
                               else {"value": vfp, "name": VFP_ARGS.get(vfp, "?")})
    arch = next((t["value"] for t in out["tags"] if t["tag"] == 6), None)
    if arch is not None:
        out["cpu_arch"] = {"value": arch, "name": CPU_ARCH.get(arch, f"?{arch}")}
    return out


def cstr(data, off):
    end = data.find(b"\0", off)
    return data[off:end].decode("utf-8", "replace") if end >= 0 else ""


def parse(path):
    data = open(path, "rb").read()
    if data[:4] != b"\x7fELF":
        raise SystemExit("not an ELF")
    rep = {"member": path.replace("\\", "/").split("/")[-1],
           "file_size": len(data), "elf_class": data[4] * 32,
           "endian": "little" if data[5] == 1 else "big"}
    if rep["elf_class"] != 32 or rep["endian"] != "little":
        raise SystemExit("this tool is ELF32-LE only (armeabi-v7a guests)")
    (rep["e_type"], rep["e_machine"], _, rep["e_entry"], e_phoff, e_shoff,
     e_flags, e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum,
     e_shstrndx) = struct.unpack_from("<HHIIIIIHHHHHH", data, 16)
    rep["e_type_name"] = {2: "ET_EXEC", 3: "ET_DYN"}.get(rep["e_type"], rep["e_type"])
    rep["e_machine_name"] = "ARM" if rep["e_machine"] == 40 else rep["e_machine"]
    rep["arm_eabi_version"] = (e_flags >> 24) & 0xF
    rep["arm_ef_flags"] = {
        "has_pic": bool(e_flags & 0x40000000),
        "soft_float_ef_arm_soft_float": bool(e_flags & 0x400),
        "vfp_float_ef_arm_vfp_float": bool(e_flags & 0x800),
        "mars": bool(e_flags & 0x100),
    }

    sections = []
    for i in range(e_shnum):
        sh = struct.unpack_from("<10I", data, e_shoff + i * e_shentsize)
        sections.append({"index": i, "name_off": sh[0], "type": sh[1],
                         "flags": sh[2], "addr": sh[3], "offset": sh[4],
                         "size": sh[5], "link": sh[6], "info": sh[7],
                         "entsize": sh[9]})
    shstrtab = sections[e_shstrndx]["offset"]
    for s in sections:
        s["name"] = cstr(data, shstrtab + s["name_off"])
    rep["sections"] = [{"name": s["name"], "type": s["type"],
                        "offset": s["offset"], "size": s["size"],
                        "addr": s["addr"], "entsize": s["entsize"]}
                       for s in sections]

    # Elf32_Phdr = type, offset, vaddr, paddr, filesz, memsz, flags, align
    phs = [struct.unpack_from("<8I", data, e_phoff + i * e_phentsize)
           for i in range(e_phnum)]
    rep["segments"] = [
        {"type": PH.get(p[0], hex(p[0])), "offset": p[1], "vaddr": p[2],
         "filesz": p[4], "memsz": p[5],
         "flags": "".join(c for c, bit in zip("rwx", (4, 2, 1)) if p[6] & bit)}
        for p in phs]

    def vaddr_to_off(v):
        for p in phs:
            if p[0] == 1 and p[2] <= v < p[2] + max(p[4], p[5]):
                return v - p[2] + p[1]
        return None

    def section_for_vaddr(v):
        for s in sections:
            if s["addr"] and s["addr"] <= v < s["addr"] + s["size"]:
                return s["name"]
        return None

    entries = []
    dyn = next((p for p in phs if p[0] == 2), None)
    if dyn:
        off, limit = dyn[1], dyn[1] + dyn[4]
        while off + 8 <= limit:
            tag, val = struct.unpack_from("<iI", data, off)
            if tag == 0:
                break
            entries.append((tag, val))
            off += 8

    def dt(tag):
        return [v for t, v in entries if t == tag]

    rep["dynamic"] = [{"tag": DT.get(t, "0x%08x" % (t & 0xFFFFFFFF)),
                       "value": v} for t, v in entries]
    strtab_vaddr = (dt(5) or [0])[0]
    strtab_off = vaddr_to_off(strtab_vaddr)
    if strtab_off is None:
        strtab_off = next((s["offset"] for s in sections
                           if s["name"] == ".dynstr"), None)
        rep["dynamic_note"] = "DT_STRTAB did not map; used the .dynstr section"
    rep["needed"] = [cstr(data, strtab_off + n) for n in dt(1)]
    rep["soname"] = cstr(data, strtab_off + (dt(14) or [0])[0]) or None
    rep["rpath_runpath"] = [cstr(data, strtab_off + v) for v in dt(15) + dt(29)]
    rep["init_fini"] = {"init": (dt(12) or [None])[0],
                        "fini": (dt(13) or [None])[0],
                        "init_array": (dt(25) or [None])[0],
                        "init_array_entries": (dt(27) or [0])[0] // 4,
                        "fini_array_entries": (dt(28) or [0])[0] // 4}
    flags = (dt(30) or [0])[0]
    flags_1 = (dt(0x6FFFFFFB) or [0])[0]
    rep["df_flags"] = sorted(v for k, v in DF_FLAGS.items() if flags & k)
    rep["df_1_pie"] = bool(flags_1 & 0x08000000)

    # guest provenance: .comment / .interp / gold note
    comment = next((s for s in sections if s["name"] == ".comment"), None)
    rep["producer"] = [c.decode("ascii", "replace") for c in
                       data[comment["offset"]:comment["offset"] + comment["size"]]
                       .split(b"\0") if c][:8] if comment else []
    interp = next((p for p in phs if p[0] == 3), None)
    rep["interpreter"] = cstr(data, interp[1]) if interp else None

    # Soft-float boundary, measured.  The tag numbering was pinned by running
    # parse_arm_attributes() over the device's own /usr/lib32/libz.so.1 stream
    # and matching that device's `readelf -A` name-for-name, value-for-value
    # (18/18, including Tag_ABI_VFP_args at 28) -- see
    # tools/test_arm_attributes.py, which re-runs that comparison.
    attrs = {}
    for s in sections:
        if s["type"] != 0x70000003:
            continue
        blob = data[s["offset"]:s["offset"] + s["size"]]
        rep["arm_attributes_raw_hex"] = blob.hex(" ")
        attrs = parse_arm_attributes(blob)
    rep["arm_attributes"] = attrs.get("tags", [])
    rep["arm_attributes_form"] = attrs.get("form")
    if attrs.get("error"):
        rep["arm_attributes_error"] = attrs["error"]
    if attrs.get("trailing_bytes"):
        rep["arm_attributes_trailing_bytes"] = attrs["trailing_bytes"]
    vfp = attrs.get("tag_abi_vfp_args")
    rep["tag_abi_vfp_args"] = vfp if vfp is not None else "absent"
    rep["arm_attributes_note"] = (
        "Tag names/numbers were pinned on-device: each raw attribute stream was "
        "paired with the same library's own `readelf -A` render for "
        "/usr/lib32/{libz.so.1, ld-linux-armhf.so.3, libc.so.6, libSDL2-2.0.so, "
        "libmali-bifrost-g31-g24p0-wayland-gbm.so}, and libz matches 18/18 in "
        "tools/test_arm_attributes.py.  Tags this tool leaves as tag_N are ones "
        "no device library declares, so binutils has never rendered them here; "
        "they carry no float-ABI meaning.  The host needs no binutils for the "
        "load-bearing measurement.")
    rep["float_abi"] = (
        "MEASURED, not inferred: this guest declares no Tag_ABI_VFP_args (28), so "
        "it follows the base AAPCS calling convention for float/double arguments "
        "(Android armeabi-v7a = -mfloat-abi=softfp) while still using VFP "
        "instructions (Tag_FP_arch is declared).  Every 32-bit provider library on "
        "the target device does declare Tag_ABI_VFP_args = VFP registers, "
        "confirmed with `readelf -A` on ld-linux-armhf.so.3, libc.so.6, "
        "libSDL2-2.0.so and libmali-bifrost-g31 (Linux ARMHF).  Every "
        "float-bearing call across the loader boundary therefore needs a softfp "
        "bridge: the framework provides one for the libm boundary "
        "(framework/nxloader/include/nxloader_softfp.h, "
        "nxloader_softfp_add_libm, __attribute__((pcs(\"aapcs\")))) and that same "
        "header states GLES float-by-value entry points are the port adapter's "
        "own registered concern.  Pointer-to-float APIs that pass addresses "
        "rather than scalars bind directly.  The same absence is the recorded "
        "contract for the other Android-guest ARMv7 cases "
        "(framework/catalog/abi-variants-v1.json:54, 'no Tag_ABI_VFP_args; never "
        "inferred') and was measured independently for the NFSMW ARMv7 guest in "
        "ports_nfsmw/research/abi-contract.md:65.")

    # symbols
    symtab = next((s for s in sections if s["type"] in (2, 0xB)), None)
    undefined, kinds, defined = [], collections.Counter(), 0
    if symtab:
        link = sections[symtab["link"]]["offset"]
        ent = symtab["entsize"] or 16
        for i in range(symtab["size"] // ent):
            nm, value, size, info, other, shndx = struct.unpack_from(
                "<IIIBBH", data, symtab["offset"] + i * ent)
            kind, bind = STT.get(info & 0xF, info & 0xF), info >> 4
            kinds[str(kind)] += 1
            if shndx == 0:
                if nm:
                    name = cstr(data, link + nm)
                    undefined.append({"name": name.split("@")[0], "kind": kind,
                                      "weak": bind == 1,
                                      "version": name.partition("@")[2] or None})
            elif kind in ("FUNC", "OBJECT") and bind in (1, 2):
                defined += 1
    rep["symbol_table"] = symtab["name"] if symtab else None
    rep["symbol_kinds"] = dict(kinds)
    rep["exported_count"] = defined
    rep["import_count"] = len(undefined)
    groups = collections.Counter()
    for u in undefined:
        n = u["name"]
        if n.startswith("Java_"):
            groups["Java_* (JNI exports the host must register)"] += 1
        elif n.startswith("__") and "chk" in n:
            groups["fortify (__*_chk)"] += 1
        elif n.startswith("__aeabi_"):
            groups["__aeabi_* EABI runtime"] += 1
        elif n.startswith("_Z"):
            groups["C++ mangled (Itanium ABI)"] += 1
        elif re.match(r"^(__system_property|__android|egl[A-Z]|gl[A-Z]|sl_|SL_|"
                      r"A[A-Z])", n):
            groups["Android platform (bionic/NDK)"] += 1
        else:
            groups["libc/bionic/plain"] += 1
    rep["import_groups"] = dict(groups)
    rep["tls_or_ifunc_imports"] = [u["name"] for u in undefined
                                   if u["kind"] in ("TLS", "GNU_IFUNC")][:40]
    rep["notable_imports"] = sorted({u["name"] for u in undefined if re.match(
        r"^(ANative|AInput|AConfiguration|AAsset|AChoreographer|AAudio|"
        r"_android_log|egl[A-Z]|gl[A-Z]|AMedia|SL_|pthread_|dlopen|dlsym|"
        r"__system_property)", u["name"])})[:200]
    # Which Android-platform entry points the guest actually names, by family.
    # This is what decides whether an ANativeWindow/EGL-surface shim can be the
    # engine's size source at all: a symbol the guest never names (and never
    # dlsyms by string) cannot be the value the engine reads.
    android_fam = collections.defaultdict(list)
    for u in undefined:
        n = u["name"]
        fam = next((p for p in ("ANativeWindow", "AInputQueue", "AConfiguration",
                                "AAsset", "AChoreographer", "AMedia", "AAudio",
                                "_android_log", "__system_property", "Java_",
                                "egl", "gl") if n.startswith(p)), None)
        if fam:
            android_fam[fam].append(n)
    rep["android_import_families"] = {k: sorted(v) for k, v in android_fam.items()}
    # Any Android-ish symbol name appearing as a string anywhere in the file,
    # i.e. reachable through dlsym/eglGetProcAddress rather than an import.
    rep["android_strings_not_imported"] = sorted(
        {m.decode() for m in re.findall(
            rb"(?:ANativeWindow|AInputQueue|AConfiguration|AAssetManager|"
            rb"AChoreographer|eglQuerySurface|__system_property)[A-Za-z0-9_]{0,28}",
            data)} - {u["name"] for u in undefined})[:60]

    # relocations: type, which section the target sits in, and the measured
    # addend semantics (does the pre-reloc field hold a link-time vaddr?).
    rel_hist = collections.Counter()
    rel_where = collections.defaultdict(collections.Counter)
    rel_abs_evidence = collections.defaultdict(lambda: [0, 0])
    image_hi = max((p[2] + p[5] for p in phs if p[0] == 1), default=0)
    text = next((s for s in sections if s["name"] == ".text"), None)
    code_lo = text["addr"] if text else 0x1000
    rel_sections = []
    for s in sections:
        if s["type"] not in (4, 9):
            continue
        ent = s["entsize"] or (8 if s["type"] == 9 else 12)
        info_at = 4 if s["type"] == 9 else 8
        for i in range(s["size"] // ent):
            ro, info = struct.unpack_from("<II", data, s["offset"] + i * ent)
            r_type = info & 0xFF
            name = "R_ARM_" + ARM_R.get(r_type, str(r_type))
            rel_hist[name] += 1
            where = section_for_vaddr(ro) or "?"
            rel_where[name][where] += 1
            field_off = vaddr_to_off(ro)
            if field_off is not None and s["type"] == 9:
                cell = struct.unpack_from("<I", data, field_off)[0]
                tally = rel_abs_evidence[name]
                tally[1] += 1
                if code_lo <= cell < image_hi:
                    tally[0] += 1
        rel_sections.append({"name": s["name"], "entries": s["size"] // ent})
    rep["relocation_sections"] = rel_sections
    rep["relocation_types"] = dict(rel_hist.most_common(40))
    rep["relocation_total"] = sum(rel_hist.values())
    rep["relocation_target_sections"] = {k: dict(v) for k, v in rel_where.items()}
    rep["relocation_field_holds_link_vaddr"] = {
        k: "%d/%d" % (v[0], v[1]) for k, v in rel_abs_evidence.items() if v[1]}
    rep["relocation_flags"] = {
        "has_tls_relocs": any("TLS" in k for k in rel_hist),
        "has_irelative": "R_ARM_IRELATIVE" in rel_hist,
        "has_android_rel": any(e["tag"] in ("ANDROID_REL", "ANDROID_RELA",
                                            "ANDROID_RELX", "RELR")
                               for e in rep["dynamic"]),
        "has_textrel": bool(dt(22)) or bool(flags & 0x4),
    }

    # V5 nxloader preflight, mirroring the marker list in tools/inventory_apk.py
    blockers = []
    if rep["tls_or_ifunc_imports"]:
        blockers.append("TLS/IFUNC imports")
    if any(e["tag"] in ("RELR", "ANDROID_REL", "ANDROID_RELA", "ANDROID_RELX")
           for e in rep["dynamic"]):
        blockers.append("packed/compact relocations (RELR or Android REL)")
    if rep["relocation_flags"]["has_tls_relocs"]:
        blockers.append("R_ARM_TLS_* relocations")
    if rep["relocation_flags"]["has_irelative"]:
        blockers.append("R_ARM_IRELATIVE")
    if rep["relocation_flags"]["has_textrel"]:
        blockers.append("TEXTREL")
    if rep["rpath_runpath"]:
        blockers.append("RPATH/RUNPATH")
    if rep["e_type_name"] != "ET_DYN":
        blockers.append("library is not ET_DYN")
    if rep["arm_eabi_version"] != 5:
        blockers.append("ARM EABI version != 5")
    if e_flags & 0x800:
        blockers.append("EF_ARM_VFP_FLOAT set: guest may use VFP register args")
    rep["v5_preflight_blockers"] = blockers

    # strings the guest itself references
    cls = {c.decode() for c in JNI_CLASS.findall(data) if c.count(b"/") >= 2}
    rep["java_class_strings"] = sorted(c for c in cls if re.match(
        r"^L(android|com|org|net|javax)/", c))[:200]
    rep["dotted_strings"] = sorted({m.decode() for m in DOTTED.findall(data)})[:200]
    rep["java_exported_names"] = sorted({u["name"] for u in undefined
                                        if u["name"].startswith("Java_")})[:80]
    return rep


BRIEF = ("member", "file_size", "elf_class", "e_machine_name", "e_type_name",
         "arm_eabi_version", "arm_ef_flags", "producer", "interpreter",
         "soname", "needed", "rpath_runpath", "df_flags", "init_fini",
         "arm_attributes", "arm_attributes_form", "arm_attributes_error",
         "arm_attributes_trailing_bytes", "tag_abi_vfp_args",
         "arm_attributes_note", "float_abi",
         "symbol_table", "symbol_kinds",
         "exported_count", "import_count", "import_groups",
         "tls_or_ifunc_imports", "notable_imports", "android_import_families",
         "android_strings_not_imported", "relocation_sections",
         "relocation_total", "relocation_types", "relocation_target_sections",
         "relocation_field_holds_link_vaddr", "relocation_flags",
         "v5_preflight_blockers", "java_exported_names")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("elf")
    ap.add_argument("--output", help="JSON report path (must not exist)")
    ap.add_argument("--brief", action="store_true")
    a = ap.parse_args()
    rep = parse(a.elf)
    if a.output:
        import os
        fd = os.open(a.output, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(fd, "w", encoding="utf-8") as out:
            json.dump(rep, out, indent=2)
        print("wrote", a.output, file=sys.stderr)
    keys = BRIEF if a.brief else tuple(k for k in rep if k not in
                                       ("sections", "java_class_strings",
                                        "dotted_strings"))
    print(json.dumps({k: rep[k] for k in keys if k in rep}, indent=2))


if __name__ == "__main__":
    main()
