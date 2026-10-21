#!/bin/sh
# run45 runner: launch, let it RUN (no early break), then report.
#
# r39g.sh stops as soon as the field dump appears, because its job was to
# catch the state at the throw.  Now that the question changed from "where
# does it die" to "how far does it get", cutting the run short is the wrong
# instrument: it kills the game right where it starts doing something
# interesting.
G=/roms/ports/beastiebay
L=$G/log.txt

echo "=== deploy + launch ==="
if [ -f /tmp/bb_newbin ]; then
  if head -c 4 /tmp/bb_newbin | od -An -c | tr -d ' \n' | grep -q '^177ELF$'; then
    cp /tmp/bb_newbin $G/beastiebay-nextos.new
    chmod +x $G/beastiebay-nextos.new
    mv -f $G/beastiebay-nextos.new $G/beastiebay-nextos
  else
    echo "ABORT: /tmp/bb_newbin nao e' ELF"
    rm -f /tmp/bb_newbin
    exit 1
  fi
fi
md5sum $G/beastiebay-nextos
: > $L
FP2_DIALOG_TRACE=1 sh /tmp/bb_launch.sh

# Give it a real run: 45 s is long enough to see whether it reaches the
# main flow, short enough not to sit on the device all night.
i=0
while [ $i -lt 45 ]; do
  i=$((i + 1))
  sleep 1
done

echo
echo "=== ran ${i}s ==="
echo
echo "=== did the error dialog happen? ==="
if grep -aq 'Utility.showDialog' $L 2>/dev/null; then
  grep -a 'Utility.showDialog' $L | head -4
  echo ">>> ERROR DIALOG PRESENT"
else
  echo ">>> no Utility.showDialog call -- error 100 did not appear"
fi
echo
echo "=== preference traffic (the branch we forced) ==="
grep -a -c 'SharedPreferences' $L 2>/dev/null
grep -a 'existPreference\|getPreference\|putPreference\|removePreference\|getPreferenceKeys' $L 2>/dev/null | head -12
echo
echo "=== throw count ==="
grep -a -c 'throw:' $L 2>/dev/null
grep -a 'throw:' $L 2>/dev/null | head -4
echo
echo "=== state machine ==="
grep -a 'state: #' $L 2>/dev/null | head -8
echo
echo "=== tail ==="
tail -14 $L
echo
for p in $(ps -eo pid,args 2>/dev/null | grep beastiebay-nextos | grep -v grep \
           | awk '{print $1}'); do
  kill -9 $p 2>/dev/null
done
echo R45_DONE
