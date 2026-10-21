#!/bin/sh
G=/roms/ports/beastiebay
echo "=== module load bases in log ==="
grep -n 'libil2cpp\|libunity.so\|libmain\|load_base\|mmap\|mapped' $G/log.txt | head -25
echo
echo "=== rotated / previous logs ==="
ls -la $G/*.txt $G/*.log* $G/log* 2>/dev/null
ls -la $G/home 2>/dev/null | head
echo
echo "=== launcher content ==="
cat "/roms/ports/Beastie Bay.sh"
echo
echo "=== env knobs referenced anywhere in log ==="
grep -o 'FP2_[A-Z_]*' $G/log.txt | sort -u
echo
echo "=== dladdr lines / any base print in loader ==="
grep -n 'dladdr\|base=\|-> lib' $G/log.txt | head -20
echo X_DONE
