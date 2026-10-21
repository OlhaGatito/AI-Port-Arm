#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Beastie Bay (Kairosoft, Unity 2018.4.24f1 IL2CPP) - AArch64 loader build.
# Structural base: ports/fp2-nextos/upstream (same engine generation, GPL-3.0-only).
set -euo pipefail

PORT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
OUTPUT=${OUTPUT:-beastiebay-nextos}
IMAGE=${IMAGE:-ghcr.io/monkeyx-net/portmaster-build-templates/portmaster-builder:aarch64-latest}

# Host path for docker -v (Windows Git-Bash / PowerShell compatible)
if [[ "$(uname -s)" == MINGW* || "$(uname -s)" == MSYS* || "$(uname -s)" == CYGWIN* ]]; then
  HOST_DIR=$(cd "$PORT_DIR" && pwd -W 2>/dev/null || echo "$PORT_DIR")
else
  HOST_DIR=$PORT_DIR
fi

export MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL="*"

docker run --rm --platform linux/arm64 \
  -v "$HOST_DIR":/src \
  -w /src \
  "$IMAGE" \
  bash -c '
    set -euo pipefail
    CC=aarch64-linux-gnu-gcc
    NM=aarch64-linux-gnu-nm
    READELF=aarch64-linux-gnu-readelf
    OUTPUT='"$OUTPUT"'
    OBJDIR=$(mktemp -d)
    STUBDIR=$(mktemp -d)
    trap "rm -rf $OBJDIR $STUBDIR" EXIT

    OBJS=()
    for source in src/*.c \
                  vendor/nxinput/src/*.c \
                  vendor/nxinput/engine-glue/*.c; do
      object="$OBJDIR/$(basename "${source%.c}").o"
      echo "CC $source"
      $CC -D_GNU_SOURCE -std=gnu11 \
        -I src \
        -I vendor/nxinput/include \
        -I vendor/nxinput/engine-glue \
        -I /usr/include/SDL2 \
        -O2 -fPIE -fno-strict-aliasing -fno-omit-frame-pointer \
        -Wno-unused-parameter -Wno-unused-result \
        -c "$source" -o "$object"
      OBJS+=("$object")
    done

    UNDEFINED=$($NM --undefined-only "${OBJS[@]}" 2>/dev/null | awk "{print \$NF}" | sort -u)

    stub_lib() {
      stub_out=$1; stub_soname=$2; stub_regex=$3
      : > "$STUBDIR/$stub_out.c"
      for symbol in $(printf "%s\n" "$UNDEFINED" | grep -E "$stub_regex" || true); do
        printf "void %s(void) {}\n" "$symbol" >> "$STUBDIR/$stub_out.c"
      done
      $CC -shared -fPIC -nostdlib -Wl,-soname,"$stub_soname" \
        "$STUBDIR/$stub_out.c" -o "$STUBDIR/lib$stub_out.so"
    }
    stub_lib SDL2 libSDL2-2.0.so.0 "^SDL_"
    stub_lib z    libz.so.1        "^(inflate|deflate|uncompress|compress|crc32|adler32|zlib|zError|gz)"

    $CC -fPIE -pie -rdynamic -static-libgcc -o "$OUTPUT" "${OBJS[@]}" \
      -L"$STUBDIR" -lSDL2 -lz -ldl -lm -lpthread

    MAX_GLIBC=$($READELF --version-info "$OUTPUT" |
      grep -oE "GLIBC_[0-9]+([.][0-9]+)*" | sort -Vu | tail -1)
    echo "GLIBC max: $MAX_GLIBC"
    ALLOWED="^(libSDL2-2\\.0\\.so\\.0|libz\\.so\\.1|libdl\\.so\\.2|libm\\.so\\.6|libpthread\\.so\\.0|libc\\.so\\.6|ld-linux-aarch64\\.so\\.1)$"
    BAD=$($READELF -d "$OUTPUT" | awk "/NEEDED/ {gsub(/[][]/,\"\",\$NF); print \$NF}" |
      grep -Ev "$ALLOWED" || true)
    if [ -n "$BAD" ]; then
      echo "FAIL: DT_NEEDED outside baseline:" >&2
      printf "  %s\n" $BAD >&2
      exit 1
    fi
    $READELF -d "$OUTPUT" | awk "/NEEDED/ {gsub(/[][]/,\"\",\$NF); printf \"  NEEDED %s\\n\", \$NF}"
    echo "BUILT $OUTPUT"
    file "$OUTPUT" || true
  '
