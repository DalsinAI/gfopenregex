# Dual library migration (19 November 2026)

The two executable commands **and** their shared Amiga libraries now have parallel names:

| Command | Shared Amiga library | SDK link stub | Engine |
| --- | --- | --- | --- |
| `C:ARegex` | `LIBS:aregex.library` v1 | `libaregex.a` | Original Team 3 MIT engine |
| `C:Regex` | `LIBS:regex.library` v2 | `libregex.a` | PCRE2 10.49, Unicode, 68040 no JIT |

**Compatibility caveat:** Existing binaries compiled against original `regex.library` v1 remain compatible with the six vector offsets and basic `regcomp/regexec` structures but may observe changed regex semantics when loading `regex.library` v2. To guarantee original behaviour, re-link against `libaregex.a`, or retain the v1 library from the historical release separately. The new `aregex.library` has original six vectors with the same register assignments, now under a different Amiga library name. **Renaming a library is not binary-transparent**; it is an explicit migration.

The new `regex.library` adds `RegSetMatchLimit` at LVO offset `0x42`, retaining six older LVO offsets `0x1e`–`0x3c`. `C:Regex` now calls PCRE2 *through its shared library*, avoiding a second embedded engine in the command. `C:ARegex` uses the separate original library. SDK includes `<regex.h>`, `libraries/regex.h`, and link stubs `libregex.a` and `libaregex.a`.

The modern shared engine uses PCRE2 10.49 with Unicode enabled and JIT disabled; it allocates compiled patterns per caller, and allocates match data per invocation. Runtime matches and limits have been qualified on isolated AmigaOS 3.2.3 AC090 68040 guest. Other concurrency, out-of-memory, malformed UTF-8, ABI migration, and hardware testing remain for 2.0 RC qualification.

**Licences:** GFOpenRegex integration/port/SDK are MIT, PCRE2 is BSD-3-Clause WITH PCRE2-exception; upstream notices are shipped verbatim. No GPL/PCRE1 material is imported.
