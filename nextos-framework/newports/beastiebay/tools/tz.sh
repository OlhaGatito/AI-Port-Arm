#!/bin/sh
L=/roms/ports/beastiebay/log.txt
echo "=== were the TimeZone binds hit? ==="
grep -n 'TimeZone\|getRawOffset' $L
echo
echo "=== 1780-1810: what immediately precedes showDialog now ==="
sed -n '1780,1810p' $L | grep -v longValue
echo
echo "=== all [fp2/sdk] in order ==="
grep -n '\[fp2/sdk\]' $L
echo TZ_DONE
