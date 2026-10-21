#!/bin/sh
# run22: GL is exonerated (GL_OES_depth24 is present in the real extension
# string, and Unity itself printed it).  What we still lack is the managed
# side's own account of the throw.  Look for every channel the managed runtime
# could have used to tell us, and for the frame handler's surroundings.
L=/roms/ports/beastiebay/log.txt
echo "=== lines 1690..1760 with numbers (error throw neighbourhood) ==="
sed -n '1690,1760p' $L | cat -n | sed 's/^/  /'
echo
echo "=== every Unity log line after 1100 ==="
awk 'NR>1100 && (/Unity/ || /AndroidRuntime/ || /DEBUG/ || /E\/|W\//)' $L | head -30
echo
echo "=== frame keep lines with context ==="
grep -n -B2 -A2 'frame [0-9]* keep=' $L | tail -40
echo R23_DONE