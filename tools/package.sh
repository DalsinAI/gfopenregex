#!/bin/sh
# Packs a build-amiga.sh build as GFOpenRegex-VERSION.lha and .zip with
# SHA256SUMS: Libs/regex.library, C/RegexTest, Tests/, SDK/ (Include, SFD,
# FD, Lib/libregex.a), Docs/. tools/package.sh VERSION [OUT]
# MIT, Copyright (c) 2026 AmiCradle Team.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
V=$1
OUT=${2:-"$HERE/dist"}
B="$HERE/build-amiga"
N="GFOpenRegex-$V"
R="$OUT/$N"
rm -rf "$R" "$OUT/$N.lha" "$OUT/$N.zip"
mkdir -p "$R/Libs" "$R/C" "$R/Tests" "$R/Docs" "$R/SDK/Include" "$R/SDK/SFD" "$R/SDK/FD" "$R/SDK/Lib"
cp "$B/lib/regex.library" "$R/Libs/"
cp "$B/tests/RegexTest" "$R/C/"
cp "$B/tests/all.txt" "$R/Tests/regex-cases.txt"
cp -R "$HERE/include/regex.h" "$HERE/include/libraries" "$HERE/include/proto" \
      "$HERE/include/clib" "$HERE/include/inline" "$R/SDK/Include/"
cp "$HERE/include/sfd/regex_lib.sfd" "$R/SDK/SFD/"
cp "$HERE/include/fd/regex_lib.fd" "$R/SDK/FD/"
cp "$B/lib/libregex.a" "$R/SDK/Lib/"
cp "$HERE/docs/regex.library.md" "$HERE/README.md" "$HERE/LICENSE" "$R/Docs/"
n=$(grep -cv '^#' "$B/tests/all.txt" || true)
COMMIT=$(git -C "$HERE" rev-parse --short HEAD 2>/dev/null || echo unknown)
cat > "$R/ReadMe.txt" <<TXT
GFOpenRegex $V
regex.library: POSIX regular expressions for AmigaOS 3.x, Perl syntax
and UTF-8 as options. Clean room. MIT licence. AmiCradle Team.

Install:   Copy Libs/regex.library LIBS:
Test:      C/RegexTest Tests/regex-cases.txt
           (expects "RegexTest: $n cases, 0 differ")
Programs:  #include <regex.h>, link SDK/Lib/libregex.a (-lregex);
           see Docs/regex.library.md.

Built from commit $COMMIT, 68040, no floating point.
TXT
(cd "$OUT" && lha -aq "$N.lha" "$N" >/dev/null && zip -qr "$N.zip" "$N" && sha256sum "$N.lha" "$N.zip" > SHA256SUMS)
cat "$OUT/SHA256SUMS"
