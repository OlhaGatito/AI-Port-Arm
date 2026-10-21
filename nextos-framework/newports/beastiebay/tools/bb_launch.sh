#!/bin/sh
echo "=== launching Beastie Bay (ArkOS) ==="
cd /roms/ports
for pid in $(ps -eo pid,args | grep -E 'beastiebay-nextos' | grep -v grep | cut -d' ' -f1); do
  echo "killing stale pid=$pid"; kill -9 $pid 2>/dev/null
done
rm -f /tmp/bb_run.out
# Pass the caller's environment explicitly: setsid detaches from the shell, so
# an exported FP2_DIALOG_TRACE would otherwise be dropped and the showDialog
# backtrace would never print.
if [ -n "$FP2_DIALOG_TRACE" ]; then
  echo "FP2_DIALOG_TRACE=$FP2_DIALOG_TRACE"
  FP2_DIALOG_TRACE="$FP2_DIALOG_TRACE" setsid './Beastie Bay.sh' > /tmp/bb_run.out 2>&1 < /dev/null &
else
  setsid './Beastie Bay.sh' > /tmp/bb_run.out 2>&1 < /dev/null &
fi
echo LAUNCHED pid=$!
