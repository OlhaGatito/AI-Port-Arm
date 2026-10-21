#!/bin/sh
# run28b: the error dialog's exact arguments. run28 proved the message is
# "An error has occurred.\n100" while metadata shows NO literal "100" anywhere in
# the managed code (const 100 has zero aligned occurrences in the il2cpp
# section), so the 100 is runtime data, not a C# literal. Print the full call
# site and the identifier before it to see where the number comes from.
L=/roms/ports/beastiebay/log.txt
echo "=== showDialog id=0 full call site (10 lines context) ==="
N=$(grep -n 'showDialog(id=0' $L | head -1 | cut -d: -f1)
sed -n "$((N>12 ? N-12 : 1)),$((N+6))p" $L
echo
echo "=== every showDialog with args, all occurrences ==="
grep -n 'showDialog' $L
echo
echo "=== does the loader itself ever inject the 100? ==="
grep -n '"100"\|100$\| 100' $L | head -10
echo
echo "=== plugin/Config.init real arguments ==="
grep -n 'Config.init\|Config.finish' $L | head -10
echo
echo "=== resource stat() outcomes: any missing asset right before the dialog? ==="
grep -n 'stat(' $L | grep -v '\-> 0' | head -25
echo
echo "=== count of stat -> -1 by suffix ==="
grep -o 'stat("[^"]*") -> -1' $L | sed 's/.*\.\([a-zA-Z]*\)") -> -1/\1/' | sort | uniq -c
echo R28B_DONE
