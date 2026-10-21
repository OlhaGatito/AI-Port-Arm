#!/usr/bin/env python3
"""Pull the Kairosoft Java plugin layer out of classes.dex without a decompiler.

Why
---
Error 100 is raised by the game after its own start-up self-check, and the
self-check is driven from kairo.android.plugin.Config.init -- the class this
port replaces with hand-written JNI stubs. The original dex that shipped inside
the APK is the authoritative reference for what that method must actually do,
and what it must RETURN. Reading it beats inferring the contract from the
crash: a stub that returns 0 where the original returns a licence string is
indistinguishable from a working one until the game checks it.

This walks the dex string, type and method tables directly, so it needs no
external tooling. It is deliberately a filter, not a full disassembler: the
question being asked is narrow ("what does Config expose, and what constants
does it mention"), and a text dump answers it without pretending to be smali.

Usage:
  dex_probe.py apk/classes.dex --class kairo/android/plugin/Config
  dex_probe.py apk/classes.dex --grep Licence
  dex_probe.py apk/classes.dex --types kairo/android/plugin
"""
import argparse
import struct
import sys


class Dex(object):
    def __init__(self, blob):
        self.blob = blob
        if blob[:4] != b"dex\n":
            raise SystemExit("not a dex file (magic %r)" % blob[:4])
        (self.string_ids_size, self.string_ids_off) = struct.unpack_from(
            "<II", blob, 0x38)
        (self.type_ids_size, self.type_ids_off) = struct.unpack_from(
            "<II", blob, 0x40)
        (self.proto_ids_size, self.proto_ids_off) = struct.unpack_from(
            "<II", blob, 0x48)
        (self.field_ids_size, self.field_ids_off) = struct.unpack_from(
            "<II", blob, 0x50)
        (self.method_ids_size, self.method_ids_off) = struct.unpack_from(
            "<II", blob, 0x58)
        (self.class_defs_size, self.class_defs_off) = struct.unpack_from(
            "<II", blob, 0x60)

    def uleb128(self, off):
        result = 0
        shift = 0
        while True:
            byte = self.blob[off]
            off += 1
            result |= (byte & 0x7F) << shift
            if not byte & 0x80:
                return result, off
            shift += 7

    def string(self, index):
        data_off, = struct.unpack_from("<I", self.blob, self.string_ids_off + index * 4)
        # MUTF-8: a ULEB128 length followed by bytes and a NUL terminator.
        size, off = self.uleb128(data_off)
        end = self.blob.find(b"\x00", off)
        raw = self.blob[off:end]
        return raw.decode("utf-8", "replace")

    def type_name(self, index):
        desc_idx, = struct.unpack_from("<I", self.blob, self.type_ids_off + index * 4)
        return self.string(desc_idx)

    def method(self, index):
        off = self.method_ids_off + index * 8
        class_idx, proto_idx, name_idx = struct.unpack_from("<HHI", self.blob, off)
        return self.type_name(class_idx), self.string(name_idx), proto_idx

    def field(self, index):
        off = self.field_ids_off + index * 8
        class_idx, type_idx, name_idx = struct.unpack_from("<HHI", self.blob, off)
        return self.type_name(class_idx), self.type_name(type_idx), self.string(name_idx)

    def classes(self):
        for i in range(self.class_defs_size):
            off = self.class_defs_off + i * 32
            class_idx, access, super_idx, interfaces_off, source_idx, \
                annotations_off, class_data_off, static_values_off = \
                struct.unpack_from("<IIIIIIII", self.blob, off)
            yield {
                "name": self.type_name(class_idx),
                "access": access,
                "super": self.type_name(super_idx) if super_idx != 0xFFFFFFFF else None,
                "class_data_off": class_data_off,
            }

    def class_members(self, class_data_off):
        """Return (static_fields, instance_fields, direct_methods, virtual_methods).

        Each field is (name_idx, type_idx); each method is (name_idx, proto_idx).
        """
        if class_data_off == 0:
            return [], [], [], []
        off = class_data_off
        sf, off = self.uleb128(off)
        inf, off = self.uleb128(off)
        dm, off = self.uleb128(off)
        vm, off = self.uleb128(off)

        def read_fields(off, count):
            out = []
            idx = 0
            for _ in range(count):
                diff, off = self.uleb128(off)
                idx += diff
                access, off = self.uleb128(off)
                out.append((idx, access))
            return out, off

        def read_methods(off, count):
            out = []
            idx = 0
            for _ in range(count):
                diff, off = self.uleb128(off)
                idx += diff
                access, off = self.uleb128(off)
                code_off, off = self.uleb128(off)
                out.append((idx, access, code_off))
            return out, off

        sfields, off = read_fields(off, sf)
        ifields, off = read_fields(off, inf)
        dmethods, off = read_methods(off, dm)
        vmethods, off = read_methods(off, vm)
        return sfields, ifields, dmethods, vmethods

    def code_item(self, code_off):
        """Return (registers, ins_size, outs_size, insns_bytes) for a method."""
        if code_off == 0:
            return None
        registers, ins, outs, tries = struct.unpack_from("<HHHH", self.blob, code_off)
        insns_size, = struct.unpack_from("<I", self.blob, code_off + 12)
        start = code_off + 16
        return registers, ins, outs, self.blob[start:start + insns_size * 2]

    def method_calls(self, code_off):
        """Yield (kind, class, name) for every invoke-* in a method body.

        Dalvik invoke instructions are 35c (register form) or 3rc (range form).
        Both carry a 16-bit method index, and the opcode is in the low byte:
            0x6e..0x72  invoke-virtual / -super / -direct / -static / -interface
        The high byte selects the form, so masking the low byte is enough to
        recognise all of them.  This is not a full disassembler: the question is
        only ever "what does this method call", and that is answerable from the
        index alone.
        """
        item = self.code_item(code_off)
        if item is None:
            return []
        _, _, _, insns = item
        out = []
        for off in range(0, len(insns) - 1, 2):
            word, = struct.unpack_from("<H", insns, off)
            op = word & 0xFF
            if op < 0x6E or op > 0x72:
                continue
            method_idx, = struct.unpack_from("<H", insns, off + 2)
            if method_idx >= self.method_ids_size:
                continue
            cls, name, _proto = self.method(method_idx)
            kind = {0x6E: "virtual", 0x6F: "super", 0x70: "direct",
                    0x71: "static", 0x72: "interface"}[op]
            out.append((kind, cls, name))
        return out


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("dex")
    ap.add_argument("--klass", dest="klass", help="dump one class: fields + methods")
    ap.add_argument("--types", help="list classes whose name contains this")
    ap.add_argument("--grep", help="list strings containing this")
    ap.add_argument("--klass-strings", dest="klass_strings", help="strings referenced by this class's code")
    args = ap.parse_args()

    blob = open(args.dex, "rb").read()
    dex = Dex(blob)
    print("dex: %d strings, %d types, %d fields, %d methods, %d classes"
          % (dex.string_ids_size, dex.type_ids_size, dex.field_ids_size,
             dex.method_ids_size, dex.class_defs_size), flush=True)

    if args.grep:
        for i in range(dex.string_ids_size):
            s = dex.string(i)
            if args.grep.lower() in s.lower():
                print("%6d  %r" % (i, s), flush=True)
        return 0

    if args.types:
        for c in dex.classes():
            if args.types in c["name"]:
                print("class %s  (super %s)" % (c["name"], c["super"]), flush=True)
        return 0

    if args.klass:
        target = args.klass
        for c in dex.classes():
            if c["name"] != target and not c["name"].endswith("/" + target):
                continue
            print("=== class %s ===" % c["name"], flush=True)
            print("    access 0x%X  super %s" % (c["access"], c["super"]), flush=True)
            sf, i_f, dm, vm = dex.class_members(c["class_data_off"])
            for label, group in (("static field", sf), ("field", i_f)):
                for idx, access in group:
                    cls, typ, name = dex.field(idx)
                    print("    %-12s %-28s %s   (access 0x%X)"
                          % (label, typ, name, access), flush=True)
            for label, group in (("direct method", dm), ("virtual method", vm)):
                for idx, access, code_off in group:
                    cls, name, proto = dex.method(idx)
                    print("    %-13s %-34s proto#%d  (access 0x%X, code 0x%X)"
                          % (label, name, proto, access, code_off), flush=True)
            return 0
        raise SystemExit("class not found: %s" % target)

    if args.klass_strings:
        for c in dex.classes():
            if args.klass_strings not in c["name"]:
                continue
            print("=== %s ===" % c["name"], flush=True)
            sf, i_f, dm, vm = dex.class_members(c["class_data_off"])
            for idx, access, code_off in dm + vm:
                cls, name, proto = dex.method(idx)
                calls = dex.method_calls(code_off)
                print("  %s.%s  (%d call sites)"
                      % (c["name"], name, len(calls)), flush=True)
                seen = []
                for kind, ccls, cname in calls:
                    key = (kind, ccls, cname)
                    if key in seen:
                        continue
                    seen.append(key)
                    print("      %-9s %s.%s" % (kind, ccls, cname), flush=True)
        return 0

    for c in dex.classes():
        if "kairo" in c["name"]:
            print("class %s" % c["name"], flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
