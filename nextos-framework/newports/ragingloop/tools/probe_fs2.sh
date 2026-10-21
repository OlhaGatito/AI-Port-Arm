echo "== mounts =="
grep -E ' /storage(/roms|/games-external)? ' /proc/mounts
echo "== case sensitivity test on roms/ports =="
cd /storage/roms/ports
rm -rf .rlt 2>/dev/null
mkdir .rlt && cd .rlt
echo A > Upper.TXT
ls
if [ -e upper.txt ]; then echo "CASE-INSENSITIVE"; else echo "CASE-SENSITIVE"; fi
echo "== exec bit =="
printf '#!/bin/sh\necho exec-ok\n' > t.sh
chmod +x t.sh
ls -l t.sh
./t.sh
echo "== symlink support =="
ln -sf t.sh link.sh 2>/dev/null && ls -l link.sh || echo "symlink FAILED"
cd /storage/roms/ports && rm -rf .rlt
echo "== block size effect =="
du -sh /storage/roms/ports/ragingloop
find /storage/roms/ports/ragingloop -type f | wc -l
stat -c '%s %b %B' /storage/roms/ports/ragingloop/assets/background/bg_001.jpg 2>/dev/null
echo FS_DONE
