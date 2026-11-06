#!/bin/sh
# Packs a build-amiga.sh build as GFOpenRegex-VERSION.lha and .zip with
# SHA256SUMS: Install-GFOpenRegex (the Amiga Installer script, with its
# icon) beside a GFOpenRegex drawer of Libs/regex.library, C/Regex, Tests/
# (RegexTest and its cases), SDK/ (Include, SFD, FD, Lib/libregex.a) and
# Docs/. tools/package.sh VERSION [OUT]
# MIT, Copyright (c) 2026 AmiCradle Team.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
V=$1
OUT=${2:-"$HERE/dist"}
B="$HERE/build-amiga"
N="GFOpenRegex-$V"
TOP="$OUT/$N"
R="$TOP/GFOpenRegex"
rm -rf "$TOP" "$OUT/$N.lha" "$OUT/$N.zip"
mkdir -p "$R/Libs" "$R/C" "$R/Tests" "$R/Docs" "$R/SDK/Include" "$R/SDK/SFD" "$R/SDK/FD" "$R/SDK/Lib"
cp "$B/lib/regex.library" "$R/Libs/"
cp "$B/c/Regex" "$R/C/"
cp "$B/tests/RegexTest" "$R/Tests/"
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

Install:   double-click Install-GFOpenRegex (beside this drawer), or
           Copy Libs/regex.library LIBS: and Copy C/Regex C:
Command:   Regex PATTERN [FILES] [EXTENDED] [PERL] [NOCASE] ... (Regex ?)
Test:      CD GFOpenRegex/Tests, then RegexTest regex-cases.txt
           (expects "RegexTest: $n cases, 0 differ")
Programs:  #include <regex.h>, link SDK/Lib/libregex.a (-lregex);
           see Docs/regex.library.md.

Built from commit $COMMIT, 68040, no floating point.
TXT
sed "s/^(set #version \"[^\"]*\")/(set #version \"$V\")/; s/^; \$VER: Install-GFOpenRegex [^ ]*/; \$VER: Install-GFOpenRegex $V/" \
    "$HERE/package/Install-GFOpenRegex" > "$TOP/Install-GFOpenRegex"
cp "$HERE/package/Install-GFOpenRegex.info" "$TOP/"
(cd "$TOP" && lha -aq "../$N.lha" GFOpenRegex Install-GFOpenRegex Install-GFOpenRegex.info >/dev/null &&
 zip -qr "../$N.zip" GFOpenRegex Install-GFOpenRegex Install-GFOpenRegex.info)
(cd "$OUT" && sha256sum "$N.lha" "$N.zip" > SHA256SUMS)
cat "$OUT/SHA256SUMS"
