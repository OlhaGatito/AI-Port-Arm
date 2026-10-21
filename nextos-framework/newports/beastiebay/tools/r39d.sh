#!/bin/sh
# run39d: read back the live bytes of every installed hook.
#
# Why read memory instead of trusting the log: three different mistakes
# produce the SAME symptom -- "hook installed, zero calls logged":
#   (a) the replacement at `target` never took effect (page still RO, or
#       something rewrote it afterwards);
#   (b) the stub ran but the way back (slot -> trampoline -> target+16)
#       landed on garbage;
#   (c) the logistics reaches the `bl` but the observed method is not the one
#       we resolved by name.
# (a) and (b) are indistinguishable from the log alone.  Reading the bytes
# settles both in one shot.
#
# THREE THINGS THIS SCRIPT HAD TO LEARN THE HARD WAY
#
# 1. ADDRESS PARSING.  The log says "hook: 0x7f9c6c16c4 -> ...".  A sed
#    pattern of \([0-9a-f]*\) stops at the 'x' of the "0x" prefix and yields
#    "0" -- so an earlier run happily printed plausible-looking words that
#    were actually read from offset ZERO.  A wrong-but-plausible read is
#    worse than no read at all.  awk with an explicit field is used instead.
#
# 2. TIMING.  The game stops its own render loop at frame 6 and exits within
#    seconds, so /proc/<pid>/mem disappears.  Fixed sleeps either miss the
#    window (read nothing) or waste a minute.  Poll for the thing actually
#    needed: either a narrated event, or the process going away.
#
# 3. CRLF.  A shell script edited on Windows with CRLF endings fails on the
#    device with "syntax error: | unexpected", because the \r becomes part
#    of the token.  Keep this file LF-only.
G=/roms/ports/beastiebay
L=$G/log.txt

echo "=== deploy + launch ==="
if [ -f /tmp/bb_newbin ]; then
  cp /tmp/bb_newbin $G/beastiebay-nextos.new
  chmod +x $G/beastiebay-nextos.new
  mv -f $G/beastiebay-nextos.new $G/beastiebay-nextos
fi
md5sum $G/beastiebay-nextos
FP2_DIALOG_TRACE=1 sh /tmp/bb_launch.sh

i=0
PID=""
while [ $i -lt 60 ]; do
  i=$((i + 1))
  sleep 1
  PID=$(ps -eo pid,args 2>/dev/null | grep beastiebay-nextos | grep -v grep \
        | cut -d' ' -f1 | head -1)
  if [ -z "$PID" ]; then
    echo "--- process gone after ${i}s"
    break
  fi
  grep -q 'state: #[0-9]' $L 2>/dev/null && break
done

echo
echo "=== pid: ${PID:-<none>}  (waited ${i}s) ==="
if [ -z "$PID" ]; then
  echo "GAME ALREADY GONE -- cannot read live bytes"
  echo R39D_DONE
  exit 0
fi

# iflag=skip_bytes,count_bytes: WITHOUT the _bytes suffix dd treats the
# number as a count of bs-sized BLOCKS -- reads the wrong offset while still
# printing plausible words, which is the worst kind of wrong.
dump() {
  printf '  %-7s %s : ' "$1" "$2"
  dd if="/proc/$PID/mem" bs=1 \
     iflag=skip_bytes,count_bytes skip="$(printf '%d' "$2")" count="$3" \
     2>/dev/null | od -An -tx4 -v | tr -s ' \n' ' ' | sed 's/^ //;s/ $//'
  echo
}

echo
echo "=== what the game reported installing ==="
grep -h 'hook:' $L | tail -6

echo
echo "=== live words at each TARGET (16 B) ==="
echo "    want: 50000050 d61f0200 <stub_lo32> <stub_hi32>"
grep -h 'hook:' $L | tail -6 | awk '{print $3}' | while read T; do
  dump target "$T" 16
done

echo
echo "=== live words at each TRAMPOLINE (32 B) ==="
echo "    want: <4 prologue words> 50000050 d61f0200 <tgt+16 lo32> <hi32>"
grep -h 'prologo copiado' $L | tail -6 | awk '{print $NF}' | tr -d ')' \
  | while read T; do
  dump tramp "$T" 32
done

echo
echo "=== narrated events ==="
grep -n 'state:' $L | tail -30
echo "--- state lines: $(grep -c 'state:' $L) ---"

for p in $(ps -eo pid,args 2>/dev/null | grep beastiebay-nextos | grep -v grep \
          | cut -d' ' -f1); do
  kill -9 $p 2>/dev/null
done
echo R39D_DONE
