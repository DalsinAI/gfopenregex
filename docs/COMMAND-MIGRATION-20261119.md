# GFOpenRegex command migration — native AmigaOS 68040 validation (19 November 2026)

**Source baseline:** existing GFOpenRegex `6c6b695123a16c2c464d4b7c0185b1cb056bdb78` (MIT Team 3 engine), unchanged `regex.library` public LVO ABI. Upstream PCRE2 tag `pcre2-10.49`, SHA `6f9d7c1373262c541324a16a358785b33ef116cf` (BSD-3-Clause WITH PCRE2-exception). MIT-licensed new command adapter in `pcre2/compat.h`. No GPL code or PCRE1 incorporated.

- `C:ARegex`: exact classic **command interface**, old `regex.library` engine; the published Team 3 v1.0-rc1 library was installed into an isolated AC090 AmigaOS 3.2.3 guest. Searches `[0-9]+` with `EXTENDED ONLY`, yields 123, 456, 789 and RC0.
- `C:Regex`: newly cross-compiled with PCRE2 10.49, Unicode enabled, JIT disabled, GCC 16.2 m68k `-m68040 -m68881 -noixemul`. Executed in the same guest; `ONLY`, `NOCASE`, `UTF8`, `LITERAL`, and `LIMIT` returned matched lines and RC0; invalid `[` pattern produced `missing terminating ]` and RC20.
- **Integrated build:** `PREFIX=<GCC16-prefix> NDK_INC=<vendor-NDK> OUT=<isolated-output> HOST_TESTS=0 BUILD_PCRE2=1 sh build-amiga.sh`: compiled `regex.library`, `libregex.a`, `C:ARegex`, and `C:Regex`, with SHA256SUMS. A clean `tools/build-pcre2-amiga.sh` rebuild separately returned RC0; installer/package script produced both LHA and ZIP and included both commands and `Docs/PCRE2-LICENCE.md`.
- Existing host engine tests still pass: `cases.txt: 106 cases, 0 differ`, `vectors.txt: 1041 cases, 0 differ` (1,147 tests total). These are tests for the original MIT engine, not exhaustive PCRE2 conformance tests.
- Guest stdout evidence: `tests/evidence/regex-pcre2-aregex-68040-guest-pass.log` for integrated new and classic commands.

**Not qualified yet:** a *separately loadable* PCRE2-backed Amiga shared library (the new `C:Regex` currently **statically links** PCRE2), mixed threading/reentrancy, every PCRE2 option, multi-line file buffering, performance on real 68040 hardware, AROS, and all PCRE2 upstream regression tests. The existing `regex.library` ABI is preserved. Do not call this a GFOpenRegex 2.0 runtime RC without those gates.

## 19 November extension: native shared-library implementation

This older command-only alpha evidence remains historically accurate. The later dual-library milestone moved the PCRE2 engine from the command binary into `LIBS:regex.library` v2; the original engine moved to `LIBS:aregex.library` v1. Modern and classic commands now dynamically open their matching libraries. Current instructions and evidence: `docs/DUAL-LIBRARY-MIGRATION-20261119.md`.
