#!/bin/sh
# Two questions the previous window could not answer:
#  1. does the managed side print an exception/stack trace anywhere?
#  2. what is the very first thing after glGetString that returns garbage?
L=/roms/ports/beastiebay/log.txt
echo "=== total lines ==="
wc -l $L
echo
echo "=== managed exception / stack evidence ==="
grep -n -i 'exception\|NullReference\|stack trace\|at kairo\|at com\.\|System\.\|Argument\|DllNotFound\|EntryPoint\|abort\|signal\|SIGABRT\|render-loop stop\|lifecycle exit\|Quit' $L | head -40
echo
echo "=== every 'unhandled' line, de-duplicated, in order ==="
grep -n 'unhandled ' $L | sed 's/^\([0-9]*\):.*unhandled /\1 /' | head -60
echo
echo "=== tail 40 ==="
tail -40 $L
echo W2_DONE