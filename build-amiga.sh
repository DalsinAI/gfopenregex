#!/bin/sh
# GFOpenRegex for AmigaOS 3.x with an os32 GCC 16 stove (bebbo's amiga-gcc,
# GCC 16, libnix).
#   STOVE  the stove (default ~/AmiCradle/stoves/os32-gcc16); or PREFIX, a
#          toolchain folder with bin/m68k-amigaos-gcc (a GFOpenAmigaGCC
#          release), with NDK_INC naming OpenNDK's Include_H
#   OUT    the output directory (default build-amiga)
#   HOST_TESTS=0  skip the PC tests (tests/run_host_tests.sh)
# Outputs: lib/regex.library (copy to LIBS:), lib/libregex.a (the link
# stub: <regex.h> and -lregex), c/ARegex (legacy command), c/Regex (PCRE2 command), include/, docs/,
# tests/RegexTest and
# tests/all.txt (the cases and vectors for RegexTest).
# The FPU check: the library and the stub use no floating point at all;
# the build fails if either links a soft-float routine. Built for the
# 68040 with the soft-float specs (a stove with fpuauto wraps any program's
# own float use; this code has none).
# MIT, Copyright (c) 2026 AmiCradle Team.
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SPECS="$HERE/specs/m68k-soft-m68040.specs"
STOVE=${STOVE:-"$HOME/AmiCradle/stoves/os32-gcc16"}
P=${PREFIX:-"$STOVE/prefix"}
CC="$P/bin/m68k-amigaos-gcc"
AR="$P/bin/m68k-amigaos-ar"
NM="$P/bin/m68k-amigaos-nm"
OUT=${OUT:-"$HERE/build-amiga"}
INC="-I$HERE/include -I$HERE/src"
[ -n "${NDK_INC:-}" ] && INC="$INC -I$NDK_INC"

if command -v python3 >/dev/null 2>&1; then
    python3 "$HERE/tools/gen_headers.py" --check
fi
if [ "${HOST_TESTS:-1}" != 0 ] && command -v "${HOST_CC:-cc}" >/dev/null 2>&1; then
    OUT="$OUT/host" sh "$HERE/tests/run_host_tests.sh"
fi

mkdir -p "$OUT/lib" "$OUT/obj" "$OUT/tests" "$OUT/docs" "$OUT/include"
cp -R "$HERE/include/." "$OUT/include/"
cp "$HERE/docs/regex.library.md" "$OUT/docs/"
cat "$HERE/tests/cases.txt" "$HERE/tests/vectors.txt" > "$OUT/tests/all.txt"

# as gfopentls and gfopenntlm: no loops turned into memset/memcpy calls,
# and address 0 is memory on an Amiga
CFLAGS="-m68040 -specs=$SPECS -Os -fno-tree-loop-distribute-patterns -fno-delete-null-pointer-checks -fomit-frame-pointer -Wall -Wextra -Werror -Wno-unused-function $INC"
CORE="rx_compile rx_exec rx_unicode rx_api"

SRC=
for f in $CORE; do SRC="$SRC $HERE/src/$f.c"; done
"$CC" $CFLAGS -mcrt=nix20 -fno-toplevel-reorder -nostartfiles -nostdlib \
  -o "$OUT/lib/aregex.library" "$HERE/library/aregex_lib.c" $SRC \
  -lgcc -Wl,-Map,"$OUT/lib/aregex.library.map"
echo "$OUT/lib/aregex.library ($(wc -c < "$OUT/lib/aregex.library") bytes)"

"$CC" $CFLAGS -noixemul -DGF_AREGEX=1 -c "$HERE/stub/regex_stub.c" -o "$OUT/obj/aregex_stub.o"
rm -f "$OUT/lib/libaregex.a"
"$AR" rcs "$OUT/lib/libaregex.a" "$OUT/obj/aregex_stub.o"
# regex.library v2 and libregex.a are built with PCRE2 below. No old ABI
# vector changes: the classic six LVOs retain their offsets; v2 adds one.
if [ "${BUILD_PCRE2:-1}" = 1 ]; then
    PREFIX="$P" OUT="$OUT" sh "$HERE/tools/build-pcre2-amiga.sh"
fi

# the FPU check
FLOAT='__(add|sub|mul|div|neg|cmp|eq|ne|lt|le|gt|ge|unord)(s|d|x)f[23]|__(fix|fixuns|float|floatun)(s|d|x)(i|d)f|__extend|__trunc|IEEEDP|IEEESP|SPFix|SPFlt'
for f in "$OUT/lib/aregex.library" "$OUT/obj/aregex_stub.o"; do
    if "$NM" "$f" 2>/dev/null | grep -E "$FLOAT" >/dev/null; then
        echo "FPU check: $f uses floating point:"; "$NM" "$f" | grep -E "$FLOAT"; exit 1
    fi
done
echo "FPU check: no floating point in aregex.library or libaregex.a"
# the library must need nothing from outside (no C library)
if "$NM" -u "$OUT/lib/aregex.library" | grep . ; then
    echo "aregex.library has undefined symbols"; exit 1
fi

"$CC" $CFLAGS -noixemul -o "$OUT/tests/RegexTest" "$HERE/tests/regex_test.c" \
  -L"$OUT/lib" -laregex
echo "$OUT/tests/RegexTest ($(wc -c < "$OUT/tests/RegexTest") bytes)"
mkdir -p "$OUT/c"
"$CC" $CFLAGS -noixemul -o "$OUT/c/ARegex" "$HERE/command/regex_cmd.c" -L"$OUT/lib" -laregex
echo "$OUT/c/ARegex ($(wc -c < "$OUT/c/ARegex") bytes)"
(cd "$OUT" && sha256sum lib/aregex.library lib/libaregex.a c/ARegex tests/RegexTest $([ "${BUILD_PCRE2:-1}" = 1 ] && printf 'lib/regex.library lib/libregex.a c/Regex') > SHA256SUMS)
cat "$OUT/SHA256SUMS"
