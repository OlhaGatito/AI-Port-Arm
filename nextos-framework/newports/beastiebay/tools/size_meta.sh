#!/bin/sh
# Size up the IL2CPP payload before pulling it: the self-check that raises
# error 100 lives in managed code, so libil2cpp.so + global-metadata.dat are the
# only places where the actual condition can be read.
D=/roms/ports/beastiebay
echo "=== IL2CPP payload ==="
ls -la $D/lib/*/libil2cpp.so 2>/dev/null
ls -la $D/assets/bin/Data/Managed/Metadata/global-metadata.dat 2>/dev/null
find $D -name 'libil2cpp.so' -o -name 'global-metadata.dat' 2>/dev/null | while read f; do
  echo "$(ls -la "$f" | awk '{print $5}')  $f"
done
echo
echo "=== unity / libunity ==="
ls -la $D/lib/*/libunity.so 2>/dev/null
echo
echo "=== free space on device ==="
df -h /tmp | tail -1
echo PROBE_SIZE_DONE
