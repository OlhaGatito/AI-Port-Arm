#!/bin/sh
G=/roms/ports/beastiebay
L=$G/log.txt
echo "=== log size / lines ==="
wc -c $L; wc -l $L
echo
echo "=== FATAL / CRASH / signal ==="
grep -n 'FATAL\|CRASH\|signum\|SIGSEGV\|assert' $L | head -20
echo
echo "=== render-loop stop / quit / lifecycle ==="
grep -n 'render-loop stop\|requested lifecycle exit\|nativeDone\|lifecycle complete\|frame .* keep' $L | tail -20
echo
echo "=== Exception / Error / showDialog ==="
grep -n 'Exception\|showDialog\|Error\b' $L | head -30
echo
echo "=== Unity managed log lines ==="
grep -n '\[?/Unity\]\|\[D/Unity\]\|\[E/Unity\]\|\[W/Unity\]\|\[V/Unity\]\|Unity)' $L | head -30
echo
echo "=== MISSLOG (miss / unhandled / null) ==="
grep -ni 'miss\|unhandled\|not bound\|missing\|fallback' $L | head -30
echo PROBE_DONE
