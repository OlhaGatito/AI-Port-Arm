#!/bin/sh
L=/roms/ports/beastiebay/log.txt
echo "=== 1690-1745: window around the Texture warning + showDialog ==="
sed -n '1690,1745p' $L
echo
echo "=== 1795-1866: last managed activity before quit ==="
sed -n '1795,1866p' $L | grep -v 'longValue'
echo
echo "=== every [fp2/sdk] line (adapter-level managed calls) ==="
grep -n '\[fp2/sdk\]' $L
echo CTX_DONE
