#!/bin/sh
echo "=== logcat: the managed side speaks here, not on our stdout ==="
logcat -d -v time 2>/dev/null | grep -i -E 'BootForm|root check|STATE_ERROR|RESLOAD|ERR\[|kairo|Unity' | tail -60
echo
echo "=== logcat: any line mentioning an error code or exception ==="
logcat -d -v time 2>/dev/null | grep -i -E 'exception|error 1|100' | tail -40
echo R30B_DONE
