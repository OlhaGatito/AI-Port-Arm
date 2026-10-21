G=/storage/roms/ports/ragingloop/assets/script
cd $G || exit 1
echo "=== files ==="
ls | head -20
echo "=== Select lines (first 40) ==="
grep -h '#Select' . 2>/dev/null | head -3
grep -rh '#Select' . | head -20
echo "=== SelectLimited ==="
grep -rh '#SelectLimited' . | head -8
echo "=== JumpC after a Select ==="
grep -rh -A3 '#Select(' . | head -30
echo "=== L lines sample ==="
grep -rh '#L(' . | head -5
