#!/bin/bash
# Stage broforce payload in NextOS layout under gamedata/broforce.
# lib: Unity 2017.4 official trio only; libstub.so / libAPKVISION.so are the
# repack-site shell (signature check / hooks) and are deliberately excluded.
set -e
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
APK="$ROOT/apk"
STAGE="$ROOT/gamedata/broforce"
rm -rf "$STAGE"
mkdir -p "$STAGE/lib" "$STAGE/assets" "$STAGE/home" "$STAGE/fonts"
for so in libmain.so libunity.so libil2cpp.so; do
  cp -a "$APK/lib/arm64-v8a/$so" "$STAGE/lib/"
done
cp -a "$APK/assets/." "$STAGE/assets/"
cp -a "$ROOT/port/fonts/." "$STAGE/fonts/" 2>/dev/null || true
echo "=== staged ==="
du -sh "$STAGE" "$STAGE/lib" "$STAGE/assets"
ls -la "$STAGE/lib"
find "$STAGE/assets" -type f | wc -l
