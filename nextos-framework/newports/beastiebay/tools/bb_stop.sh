#!/bin/sh
echo "=== running instances ==="
ps -eo pid,ppid,stat,args | grep -E 'beastiebay-nextos|Beastie Bay' | grep -v grep
for pid in $(ps -eo pid,args | grep -E 'beastiebay-nextos' | grep -v grep | cut -d' ' -f1); do
  echo "killing pid=$pid"; kill -9 $pid 2>/dev/null
done
sleep 2
echo "=== after kill ==="
ps -eo pid,args | grep -E 'beastiebay-nextos' | grep -v grep | wc -l
echo DONE
