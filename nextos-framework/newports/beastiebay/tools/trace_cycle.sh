#!/bin/sh
# run25: IL2CPP managed-exception tracing.
#  FP2_IL2_EXCEPTION_TRACE - wrap il2cpp_raise_exception / _unhandled_exception
#                            through the bionic dlsym shim, and print the
#                            formatted managed exception (type + message + stack)
#  FP2_DIALOG_TRACE        - native backtrace at showDialog
# Both are diagnostic-only switches, off unless requested.
L=/roms/ports/beastiebay/log.txt
rm -f /tmp/bb_run.out
for pid in $(ps -eo pid,args | grep -E 'beastiebay-nextos' | grep -v grep | cut -d' ' -f1); do
  kill -9 $pid 2>/dev/null
done
cd /roms/ports || exit 1
FP2_DIALOG_TRACE=1 FP2_IL2_EXCEPTION_TRACE=1 \
  setsid './Beastie Bay.sh' > /tmp/bb_run.out 2>&1 < /dev/null &
echo "launched pid=$!"
sleep 50
for pid in $(ps -eo pid,args | grep -E 'beastiebay-nextos' | grep -v grep | cut -d' ' -f1); do
  kill -9 $pid 2>/dev/null
done
echo "=== hooks installed:"
grep -n 'hooked il2cpp' $L
echo "=== managed exceptions seen:"
grep -c 'fp2/il2' $L
grep -n -A4 'fp2/il2\] raise_exception\|fp2/il2\] unhandled' $L | head -60
echo "=== showDialog backtraces:"
grep -A8 'showDialog id=' $L | head -30
echo "=== error 100 count:"
grep -c 'An error has occurred' $L
echo R25_DONE