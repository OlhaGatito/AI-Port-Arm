#!/bin/sh
# Cross-build the Raging Loop loader inside the PortMaster aarch64 image.
#   sh tools/build.sh          build + report arch/NEEDED/GLIBC
#   sh tools/build.sh clean    remove port/build-aarch64
# The host installs nothing; artifacts stay under port/build-aarch64.
set -e

IMAGE=ghcr.io/monkeyx-net/portmaster-build-templates/portmaster-builder:aarch64-latest
SRC=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$SRC/.." && pwd)
OUT="$ROOT/port/build-aarch64"

if [ "$1" = clean ]; then
	rm -rf "$OUT"
	echo "cleaned $OUT"
	exit 0
fi

mkdir -p "$OUT"
# MSYS_NO_PATHCONV: Git-Bash otherwise rewrites the /src mount spec into a
# Windows path (the same trap that moved the first payload upload).
MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*' docker run --rm \
	-v "$ROOT/port":/src -w /src "$IMAGE" sh /src/build-inner.sh

echo "artifact: $OUT/ragingloop"
