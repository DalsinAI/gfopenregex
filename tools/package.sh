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
B=${BUILD_OUTPUT:-"$HERE/build-amiga"}
N="GFOpenRegex-$V"
TOP="$OUT/$N"
R="$TOP/GFOpenRegex"
rm -rf "$TOP" "$OUT/$N.lha" "$OUT/$N.zip"
mkdir -p "$R/Libs" "$R/C" "$R/Tests" "$R/Docs" "$R/SDK/Include" "$R/SDK/SFD" "$R/SDK/FD" "$R/SDK/Lib"
cp "$B/lib/regex.library" "$B/lib/aregex.library" "$R/Libs/"
cp "$B/c/ARegex" "$B/c/Regex" "$R/C/"
cp "$B/tests/RegexTest" "$R/Tests/"
cp "$B/tests/all.txt" "$R/Tests/regex-cases.txt"
cp -R "$HERE/include/regex.h" "$HERE/include/libraries" "$HERE/include/proto" \
      "$HERE/include/clib" "$HERE/include/inline" "$R/SDK/Include/"
cp "$HERE/include/sfd/regex_lib.sfd" "$R/SDK/SFD/"
cp "$HERE/include/fd/regex_lib.fd" "$R/SDK/FD/"
cp "$B/lib/libregex.a" "$B/lib/libaregex.a" "$R/SDK/Lib/"
cp "$HERE/docs/regex.library.md" "$HERE/docs/REGEX-PCRE2-COMMAND.md" "$HERE/docs/DUAL-LIBRARY-MIGRATION-20261119.md" "$HERE/README.md" "$HERE/LICENSE" "$R/Docs/"
cp "$HERE/pcre2/PCRE2-LICENCE.md" "$R/Docs/PCRE2-LICENCE.md"
n=$(grep -cv '^#' "$B/tests/all.txt" || true)
COMMIT=${SOURCE_COMMIT:-$(git -C "$HERE" rev-parse --short HEAD 2>/dev/null || echo unknown)}
cat > "$R/ReadMe.txt" <<TXT
GFOpenRegex $V
aregex.library: original MIT regex engine and POSIX/Perl regex API.
regex.library: new PCRE2 10.49 backend, extended matching options.
Original Team 3 library and our additions are MIT.
C:ARegex: original command, uses classic MIT aregex.library.
C:Regex: PCRE2 10.49-backed replacement, with modern pattern syntax.
The new shared regex.library contains PCRE2 under BSD-3-Clause +
PCRE2-exception; our integration is MIT; see Docs/PCRE2-LICENCE.md.

Install:   double-click Install-GFOpenRegex (beside this drawer), or
           Copy Libs/aregex.library and Libs/regex.library to LIBS:; Copy C/ARegex and C/Regex C:
Command:   Regex PATTERN [FILES] [EXTENDED] [PERL] [NOCASE] ... (Regex ?)
Test:      CD GFOpenRegex/Tests, then RegexTest regex-cases.txt
           (expects "RegexTest: $n cases, 0 differ")
Programs:  #include <regex.h>; link -lregex for PCRE2, -laregex for legacy.
           see Docs/regex.library.md.

Built from commit $COMMIT, 68040. No 68k regex JIT.
TXT
sed "s/^(set #version \"[^\"]*\")/(set #version \"$V\")/; s/^; \$VER: Install-GFOpenRegex [^ ]*/; \$VER: Install-GFOpenRegex $V/" \
    "$HERE/package/Install-GFOpenRegex" > "$TOP/Install-GFOpenRegex"
cp "$HERE/package/Install-GFOpenRegex.info" "$TOP/"
(cd "$TOP" && lha -aq "../$N.lha" GFOpenRegex Install-GFOpenRegex Install-GFOpenRegex.info >/dev/null &&
 zip -qr "../$N.zip" GFOpenRegex Install-GFOpenRegex Install-GFOpenRegex.info)
(cd "$OUT" && sha256sum "$N.lha" "$N.zip" > SHA256SUMS)
cat "$OUT/SHA256SUMS"
