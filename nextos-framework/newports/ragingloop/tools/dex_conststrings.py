#!/usr/bin/env python3
"""Ordered const-string extractor for DEX methods.

The ragingloop Java engine bakes its asset index tables into String[] literals,
so the order of const-string operands inside a method IS the id->file mapping.
This walks code_items with the Dalvik instruction-width table and prints the
ordered string references per method. Alignment is self-checked: a correct walk
lands exactly on the end of every code_item.

  python dex_conststrings.py apk/classes.dex background/bg_001
  python dex_conststrings.py apk/classes.dex "01/0_brow" --dump
"""
import struct
import sys

# instruction width in 16-bit code units, indexed by opcode; 0 = unused
W = [0] * 256
_ONE = ([0x00, 0x01, 0x04, 0x07, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
         0x11, 0x12, 0x1C, 0x20, 0x26, 0x2C] +
        list(range(0x3F, 0x46)) + [0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C] +
        list(range(0x4D, 0x62)) + list(range(0x62, 0x6E)) +
        [0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85,
         0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D] +
        list(range(0xB0, 0xDC)) + [0xDC, 0xDD, 0xDE, 0xDF])
_TWO = ([0x02, 0x03, 0x05, 0x06, 0x08, 0x09, 0x13, 0x14, 0x15, 0x18, 0x19,
         0x1A, 0x1B, 0x1D, 0x1E, 0x1F, 0x21, 0x22, 0x23, 0x27] +
        list(range(0x32, 0x3F)))
_THREE = ([0x16, 0x17, 0x24, 0x25, 0x28, 0x29, 0x2A, 0x2B, 0x59, 0x5A, 0x5B,
           0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
           0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71] +
          list(range(0xF0, 0xFB)))
for op in _TWO:
    W[op] = 2
for op in _THREE:
    W[op] = 3
W[0x17] = 5           # const-wide
W[0x29] = 4           # goto/32
W[0x1B] = 3           # const-string/jumbo
for op in _ONE:
    W[op] = 1


class Dex:
    def __init__(self, path):
        self.buf = open(path, "rb").read()
        if self.buf[:4] != b"dex\n":
            raise SystemExit("not a dex: %s" % path)
        (self.sis, self.so, self.tis, self.to, _pis, _po, _fis, _fo,
         self.mis, self.mo, self.cds, self.cdo) = \
            struct.unpack_from("<12I", self.buf, 56)
        self.strings = [self.cstring(
            struct.unpack_from("<I", self.buf, self.so + 4 * i)[0])
            for i in range(self.sis)]
        self.types = [self.strings[struct.unpack_from("<I", self.buf,
                                                     self.to + 4 * i)[0]]
                     for i in range(self.tis)]

    def uleb(self, off):
        val = shift = 0
        while True:
            b = self.buf[off]
            off += 1
            val |= (b & 0x7F) << shift
            if not (b & 0x80):
                return val, off
            shift += 7

    def cstring(self, off):
        _, off = self.uleb(off)
        end = self.buf.index(b"\x00", off)
        return self.buf[off:end].decode("utf-8", "replace")

    def method_id(self, idx):
        # method_id_item: ushort class_idx | ushort proto_idx | uint name_idx
        cidx, _proto, nidx = struct.unpack_from("<HHI", self.buf,
                                                self.mo + 8 * idx)
        return self.strings[nidx], self.types[cidx]

    def methods(self):
        """[(class, name, code_off, insns_size)] for every method with code."""
        out = []
        for i in range(self.cds):
            base = self.cdo + 32 * i
            vals = struct.unpack_from("<7I", self.buf, base)
            cls = self.types[vals[0]]
            cdata = vals[6]
            if not cdata:
                continue
            p = cdata
            nfs, p = self.uleb(p)
            nif, p = self.uleb(p)
            ndirect, p = self.uleb(p)
            nvirtual, p = self.uleb(p)
            for _ in range(nfs + nif):          # encoded_field: 2 ulebs each
                _fidx, p = self.uleb(p)
                _acc, p = self.uleb(p)
            idx = 0
            for count in (ndirect, nvirtual):
                idx, p = _read_methods(self, cls, count, p, out, idx)
        return out


def _read_methods(dex, cls, count, p, out, idx):
    idx = 0
    for _ in range(count):
        d, p = dex.uleb(p)
        idx += d
        _access, p = dex.uleb(p)
        coff, p = dex.uleb(p)
        if coff:
            insns_size, = struct.unpack_from("<I", dex.buf, coff + 12)
            mname, mtype = dex.method_id(idx)
            out.append((cls, mname, coff + 16, insns_size))
    return idx, p


def const_strings(dex, code_off, insns_size):
    buf = dex.buf
    out = []
    bad = 0
    i = 0
    while i < insns_size:
        unit, = struct.unpack_from("<H", buf, code_off + 2 * i)
        op = unit & 0xFF
        if op == 0x1A:  # const-string
            idx, = struct.unpack_from("<H", buf, code_off + 2 * (i + 1))
            out.append(dex.strings[idx] if idx < len(dex.strings) else "?")
            i += 2
            continue
        if op == 0x00 and i + 1 < insns_size:
            ident, = struct.unpack_from("<H", buf, code_off + 2 * (i + 1))
            if ident in (0x0100, 0x0200, 0x0300):
                if ident == 0x0100:
                    size, = struct.unpack_from("<H", buf, code_off + 2 * (i + 2))
                    i += 4 + 2 * size
                elif ident == 0x0200:
                    size, = struct.unpack_from("<H", buf, code_off + 2 * (i + 2))
                    i += 4 + 4 * size
                else:
                    width, = struct.unpack_from("<H", buf, code_off + 2 * (i + 2))
                    size, = struct.unpack_from("<I", buf, code_off + 2 * (i + 3))
                    data = code_off + 2 * i + 8
                    elem = max(1, width // 2) if width != 8 else 8
                    if elem == 8:
                        data = (data + 7) & ~7
                    consumed = (data - (code_off + 2 * i)) // 2 + (size * 8) // 2 \
                        if elem == 8 else \
                        (data - (code_off + 2 * i)) // 2 + (size * elem + 1) // 2
                    i += consumed
                if i > insns_size:
                    bad += 1
                    break
                continue
        w = W[op]
        if w == 0:
            bad += 1
            i += 1
            continue
        i += w
    return out, bad, i == insns_size


def main():
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    path = sys.argv[1]
    needle = sys.argv[2] if len(sys.argv) > 2 else "background/bg_001"
    dump = "--dump" in sys.argv
    dex = Dex(path)
    rows = dex.methods()
    print("methods with code=%d" % len(rows))
    hits = []
    total_bad = exact_n = 0
    for cls, mname, code_off, insns_size in rows:
        strs, bad, exact = const_strings(dex, code_off, insns_size)
        total_bad += bad
        exact_n += 1 if exact else 0
        if any(needle in s for s in strs):
            hits.append((cls, mname, strs))
    print("walk alignment exact=%d/%d  bogus=%d" % (exact_n, len(rows), total_bad))
    print("methods referencing %r: %d" % (needle, len(hits)))
    for cls, mname, strs in hits:
        print("\n### %s .%s  const-strings=%d" % (cls, mname, len(strs)))
        if dump:
            for j, s in enumerate(strs):
                print("%5d  %s" % (j, s))
        else:
            print("   %s" % " | ".join(strs[:50]))


main()
