# GFOpenRegex dual Amiga shared-library UAT — 19 November 2026

**Test environment:** disposable AC090 AmigaOS 3.2.3 68040 guest; OpenAmigaGCC 16.2, m68k; isolated under `/mnt/xs1000/oolite-sdl3-input-uat-20261118`; historical guest at ports 9140/9141 untouched.

**Source:** DalsinAI/gfopenregex main from `444670c91fd8ceb0f6e6e6851065f802bd42891b` plus current shared-library migration; original MIT Team 3 engine has no PCRE1 dependency; PCRE2 10.49 pinned upstream commit `6f9d7c1373262c541324a16a358785b33ef116cf` under BSD-3-Clause WITH PCRE2-exception; MIT port.

**Build:** `PREFIX=... NDK_INC=... OUT=... HOST_TESTS=0 BUILD_PCRE2=1 sh build-amiga.sh` exit 0, emitting `LIBS:aregex.library` v1 (~20KB), `LIBS:regex.library` v2 (~279KB), `libaregex.a`, `libregex.a`, `C:ARegex`, `C:Regex`, and `RegexTest`. Both libraries report zero undefined symbols after linking. Old engine 1147 host vectors passed (106 + 1041). All seven distributable outputs checksum verified against `SHA256SUMS`.

**Native guest:** copied both libraries to isolated `DH0/Libs` and commands to isolated `DH0/ACAgent/RegexUAT`.

- `Regex "[0-9]+" ascii.txt ONLY LIMIT=100000` -> `123`, `456`, `789`, RC0 (new shared PCRE2 library, dynamically loaded).
- `ARegex "[0-9]+" ascii.txt EXTENDED ONLY` -> same lines, RC0 (original engine renamed shared library, dynamically loaded).
- `RegexTest all.txt` -> **1147 cases, 0 differ**, RC0 with `aregex.library` (actual guest suite).
- New library `Regex` with `(?<word>[A-Za-z]+)` -> captures matched words, RC0; `UTF8 UCP` matches both `Cafe` and `Café`, RC0; invalid `[` -> RC20; nonmatching literal -> RC5.

**Evidence files:** `tests/evidence/aregex-library-1147-68040-guest-pass.log` and `tests/evidence/regex-pcre2-shared-library-68040-guest-pass.log`.

**Remaining qualification:** guest library version/open/close under concurrent callers, allocator fault injection, UTF-8 malformed data and pathological match limits, real 68040 hardware, AROS compatibility, semantic migration of old `regex.library`-linked applications. No claim of full PCRE2 upstream test-suite completion. The Amiga Installer script is packaged but its full interactive Workbench workflow has not been exercised.
