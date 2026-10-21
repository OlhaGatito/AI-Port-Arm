#!/usr/bin/env bash
# Nearest guest symbol below an address, from .dynsym only.
#
# The engine library is stripped (no .symtab), so this answers "which exported
# function owns this offset" rather than a full backtrace. Used to turn a
# SIGSEGV's `lr = module+0x…` into a name.
#
#   tools/guest_symbolize.sh <libRealRacing3.so> <hex-offset> [<hex-offset> …]
#
# Comparisons are done on the zero-padded 8-hex-digit fields readelf prints:
# for equal-length hex, string order is numeric order. Deliberately no awk
# strtonum() - the build image's awk is mawk, which does not have it.
set -u
SO=$1; shift
READELF=${READELF:-arm-linux-gnueabihf-readelf}
WIDTH=8

TABLE=$(mktemp)
trap 'rm -f "$TABLE"' EXIT

"$READELF" -sW "$SO" 2>/dev/null \
  | awk -v w="$WIDTH" 'NR>3 && length($2)==w && $2 ~ /^[0-9a-f]+$/ && $4!="NOTYPE" && $8!="" \
                       {print $2, $8}' \
  | sort -u -k1,1 > "$TABLE"

for addr in "$@"; do
  q=$(printf '%0*s' "$WIDTH" "$addr")
  printf '=== module+0x%s ===\n' "$addr"
  awk -v q="$q" '$1 <= q { last = $1 " " $2 } END {
      if (last == "") print "  (no exported symbol at or below this address)"
      else print "  " last }' "$TABLE"
done
