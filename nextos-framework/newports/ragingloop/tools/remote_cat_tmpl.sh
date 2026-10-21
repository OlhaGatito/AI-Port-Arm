cd /storage/roms/ports || exit 1
ls -1 *.sh | head -20
echo "########## PvzPortable.sh"
sed -n '1,80p' PvzPortable.sh 2>/dev/null
