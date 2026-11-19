# Upstream PCRE2 10.49 (not PCRE1)

Upstream: https://github.com/PCRE2Project/pcre2
Tag: `pcre2-10.49`
Pinned Git SHA: `6f9d7c1373262c541324a16a358785b33ef116cf`
Upstream licence: `BSD-3-Clause WITH PCRE2-exception`, exact upstream notices in `pcre2/PCRE2-LICENCE.md`.

The upstream PCRE2 source is **not copied into GitHub history**: build script retrieves and validates the tag SHA, builds an 8-bit Unicode-enabled static engine with JIT disabled for m68k, and links it to the MIT-licensed `C:Regex` AmigaDOS command. `C:ARegex` and the existing MIT `regex.library` remain unchanged ABI-wise. No GPL or PCRE1 code is imported. There is not yet a PCRE2-backed shared Amiga library.
