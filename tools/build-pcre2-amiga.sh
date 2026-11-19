#!/bin/sh
# MIT (integration), upstream PCRE2 10.49 BSD-3-Clause WITH PCRE2-exception.
# Reproducible native 68040 PCRE2 command: no 68k JIT, Unicode enabled.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
P=${PREFIX:?Set PREFIX to the GCC 16.2 toolchain prefix}
OUT=${OUT:-"$HERE/build-amiga"}
SRC=${PCRE2_SRC:-"$HERE/third_party/pcre2-10.49"}
PIN=6f9d7c1373262c541324a16a358785b33ef116cf
if [ ! -f "$SRC/CMakeLists.txt" ]; then
    command -v git >/dev/null || { echo 'git is needed to fetch PCRE2'; exit 2; }
    mkdir -p "$(dirname "$SRC")"
    git clone -q --depth 1 --branch pcre2-10.49 https://github.com/PCRE2Project/pcre2.git "$SRC"
fi
if [ -d "$SRC/.git" ]; then
    got=$(git -C "$SRC" rev-parse HEAD)
    [ "$got" = "$PIN" ] || { echo "PCRE2 source SHA $got differs from pinned $PIN"; exit 2; }
fi
B="$OUT/pcre2-build"
cmake -S "$SRC" -B "$B" -DCMAKE_SYSTEM_NAME=Generic \
  -DCMAKE_C_COMPILER="$P/bin/m68k-amigaos-gcc" \
  -DCMAKE_AR="$P/bin/m68k-amigaos-ar" -DCMAKE_RANLIB="$P/bin/m68k-amigaos-ranlib" \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
  -DCMAKE_C_FLAGS='-Os -m68040 -m68881 -noixemul' -DBUILD_SHARED_LIBS=OFF \
  -DPCRE2_BUILD_PCRE2_8=ON -DPCRE2_BUILD_PCRE2_16=OFF \
  -DPCRE2_BUILD_PCRE2_32=OFF -DPCRE2_BUILD_PCRE2GREP=OFF \
  -DPCRE2_BUILD_TESTS=OFF -DPCRE2_SUPPORT_JIT=OFF \
  -DPCRE2_SUPPORT_UNICODE=ON -DPCRE2_BUILD_PCRE2POSIX=OFF
cmake --build "$B" --target pcre2-8-static -j "${JOBS:-4}"
mkdir -p "$OUT/c" "$OUT/lib"
"$P/bin/m68k-amigaos-gcc" -Os -m68040 -m68881 -noixemul \
  -Wall -Wextra -Wno-unused-function -DGF_PCRE2=1 \
  -I"$P/m68k-amigaos/ndk-include.vendor" -I"$B/interface" \
  "$HERE/command/regex_cmd.c" "$B/libpcre2-8.a" -o "$OUT/c/Regex"
cp "$B/libpcre2-8.a" "$OUT/lib/libpcre2-8.a"
file "$OUT/c/Regex"
echo "PCRE2 Regex command: $OUT/c/Regex"
