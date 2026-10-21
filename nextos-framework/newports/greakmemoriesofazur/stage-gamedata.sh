#!/bin/bash
# Stage greak payload in NextOS layout under gamedata/greak
set -e
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
APK="$ROOT/apk"
STAGE="$ROOT/gamedata/greak"
rm -rf "$STAGE"
mkdir -p "$STAGE/lib" "$STAGE/assets" "$STAGE/home"
cp -a "$APK/lib/arm64-v8a/." "$STAGE/lib/"
cp -a "$APK/assets/." "$STAGE/assets/"
echo "=== staged ==="
du -sh "$STAGE" "$STAGE/lib" "$STAGE/assets"
ls -la "$STAGE/lib"
find "$STAGE/assets" -type f | wc -l
