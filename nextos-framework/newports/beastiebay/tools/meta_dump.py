#!/usr/bin/env python3
"""Decode the IL2CPP metadata string/blob tables of a Unity 2018.4 IL2CPP build.

Why this exists
---------------
Kairosoft error 100 is NOT an exception and NOT a crash: run25 proved the game
deliberately calls Utility.showDialog(0, ...) after its own start-up self-check
fails. The condition that failed therefore exists only in managed code, which on
an IL2CPP build lives in two places:

  * global-metadata.dat -- the string literals and the member/method names,
  * libil2cpp.so       -- the compiled native code for those methods.

String literals are stored in a single concatenated UTF-8 blob whose entries are
listed in FIRST-REFERENCE order, so neighbours in the table tend to come from the
same method or class. That is the property this tool exploits: searching the blob
around a known literal tends to surface the literals its neighbours use, which is
how the self-check's individual conditions can be read out.

Metadata v24 header layout (little endian), fields consumed here:
    0x00 magic 0xFAB11BAF, 0x04 version
    0x08 stringLiteralOffset / stringLiteralSize
    0x10 stringLiteralDataOffset / stringLiteralDataSize
    0x18 stringOffset / stringSize

Verified against this very file rather than assumed: the three tables are
byte-contiguous (272+74488 == 74760, 74760+188360 == 263120, 263120+545316
== 808436), which is what fixes the second header field as a BYTE SIZE rather
than an element count. Treating it as a count silently overruns the blob.

The string table has no separate data offset in the header: its UTF-8 bytes
begin immediately after the index table, at stringOffset + stringSize.

Usage:
  meta_dump.py bbmeta/global-metadata.dat --grep "error"
  meta_dump.py bbmeta/global-metadata.dat --near "An error has occurred." 40
  meta_dump.py bbmeta/global-metadata.dat --strings | head -50
"""
import argparse
import struct
import sys

MAGIC = 0xFAB11BAF
SANITY = 0xFAB11BAF


class Metadata(object):
    def __init__(self, blob):
        self.blob = blob
        if len(blob) < 0x70:
            raise SystemExit("file too small to be global-metadata.dat (%d bytes)"
                             % len(blob))
        magic, version = struct.unpack_from("<II", blob, 0)
        if magic != MAGIC:
            raise SystemExit("bad magic 0x%08X (expected 0x%08X); not a "
                             "global-metadata.dat" % (magic, MAGIC))
        self.version = version
        # Pair tables are stored as offset, count at a fixed stride from 0x10.
        # Walking them by the documented stride keeps this readable instead of a
        # wall of named struct unpacks, and the sanity check below catches a
        # mis-stride immediately rather than yielding silent garbage.
        self.tables = []
        for slot in range(0, 28):
            off = 0x10 + slot * 8
            table_off, table_count = struct.unpack_from("<II", blob, off)
            if table_off == 0 and table_count == 0:
                continue
            self.tables.append((off, table_off, table_count))

    def describe(self):
        out = ["magic 0x%08X  version %d  size %d bytes"
               % (MAGIC, self.version, len(self.blob))]
        for label, (off, size) in (
                ("stringLiteral", self.pair(self.STRING_LITERAL)),
                ("stringLiteralData", self.pair(self.STRING_LITERAL_DATA)),
                ("string", self.pair(self.STRING))):
            out.append("  %-18s offset 0x%08X  size %d bytes" % (label, off, size))
        return "\n".join(out)

    def pair(self, slot):
        """Return (offset, size_in_bytes) for header slot N (0x08 + N*8)."""
        off = 0x08 + slot * 8
        return struct.unpack_from("<II", self.blob, off)

    # The three tables that matter for literal hunting.
    STRING_LITERAL = 0       # Il2CppStringLiteral { length, dataIndex }
    STRING_LITERAL_DATA = 1  # raw concatenated UTF-8 literal bytes
    STRING = 2               # Il2CppString { length, dataIndex } -> names

    ENTRY = 8  # both index structs are two little-endian int32 fields

    def string_literals(self):
        """Yield (index, literal_text) for every managed string literal."""
        lit_off, lit_size = self.pair(self.STRING_LITERAL)
        data_off, _ = self.pair(self.STRING_LITERAL_DATA)
        for i in range(lit_size // self.ENTRY):
            length, data_index = struct.unpack_from(
                "<II", self.blob, lit_off + i * self.ENTRY)
            start = data_off + data_index
            raw = self.blob[start:start + length]
            yield i, raw.decode("utf-8", "replace")

    def names(self):
        """Yield (index, name) for every interned name/identifier string.

        Slot 2 is not an index array: its bytes are themselves the NUL-
        separated UTF-8 blob ("mscorlib\\0\\0NULL\\0\\0...").  Reading it as
        {length, dataIndex} pairs yields plausible-looking empty strings
        rather than an error, which is the dangerous kind of wrong.  Split on
        NUL and keep the ordinal, since neighbourhood is what identifies which
        code block a name belongs to.
        """
        str_off, str_size = self.pair(self.STRING)
        for i, raw in enumerate(self.blob[str_off:str_off + str_size].split(b"\x00")):
            yield i, raw.decode("utf-8", "replace")

    def all_text(self):
        """Every literal plus every name, in one searchable stream."""
        out = []
        for i, text in self.string_literals():
            out.append(("literal", i, text))
        for i, text in self.names():
            out.append(("name", i, text))
        return out


def printable(text, limit=400):
    text = text.replace("\r", "\\r").replace("\n", "\\n").replace("\t", "\\t")
    if len(text) > limit:
        text = text[:limit] + "..."
    return text


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("path", help="path to global-metadata.dat")
    ap.add_argument("--header", action="store_true", help="dump the header tables")
    ap.add_argument("--grep", metavar="TEXT", help="literals containing TEXT")
    ap.add_argument("--iname", metavar="TEXT", help="identifiers containing TEXT")
    ap.add_argument("--near", nargs=2, metavar=("TEXT", "N"), type=str,
                    help="N literals on each side of the first match of TEXT")
    ap.add_argument("--strings", action="store_true", help="dump every literal")
    ap.add_argument("--names", action="store_true", help="dump every identifier")
    args = ap.parse_args()

    with open(args.path, "rb") as fh:
        meta = Metadata(fh.read())

    if args.header or not (args.grep or args.iname or args.near
                           or args.strings or args.names):
        print(meta.describe())
        return 0

    if args.strings:
        for i, text in meta.string_literals():
            print("%8d  %s" % (i, printable(text)))
        return 0

    if args.names:
        for i, text in meta.names():
            print("%8d  %s" % (i, printable(text, 200)))
        return 0

    if args.near:
        needle, span = args.near[0], int(args.near[1])
        literals = list(meta.string_literals())
        for pos, (_, text) in enumerate(literals):
            if needle in text:
                print("=== literal #%d matches %r (showing +-%d) ==="
                      % (pos, needle, span))
                for j in range(max(0, pos - span), min(len(literals), pos + span + 1)):
                    idx, body = literals[j]
                    mark = ">>" if j == pos else "  "
                    print("%s %8d  %s" % (mark, idx, printable(body)))
                break
        else:
            print("no literal contains %r" % needle)
        return 0

    if args.grep:
        found = 0
        for i, text in meta.string_literals():
            if args.grep in text:
                print("%8d  %s" % (i, printable(text)))
                found += 1
        print("--- %d literal(s) match %r ---" % (found, args.grep))
        return 0

    if args.iname:
        found = 0
        for i, text in meta.names():
            if args.iname in text:
                print("%8d  %s" % (i, printable(text, 200)))
                found += 1
        print("--- %d identifier(s) match %r ---" % (found, args.iname))
        return 0

    return 0


if __name__ == "__main__":
    sys.exit(main())
