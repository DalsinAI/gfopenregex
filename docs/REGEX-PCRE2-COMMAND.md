# C:Regex and C:ARegex — modern engine without legacy breakage

`C:ARegex` is the original GFOpenRegex 1.0 grep-style command, with its command template and `regex.library` ABI unchanged. It continues to use the MIT-licensed Team 3 engine. Existing client code linked to `regex.library` is untouched.

`C:Regex` is the replacement command. It is MIT-licensed original AmigaDOS glue linked with pinned upstream **PCRE2 10.49**, in the 8-bit library configuration with Unicode support and no JIT for 68040. The PCRE2 matching engine retains upstream `BSD-3-Clause WITH PCRE2-exception` licensing and copyright notices (shipped as `Docs/PCRE2-LICENCE.md`). **No PCRE1 or GPL wrapper code is used.**

Both commands accept `PATTERN`, `FILES`, `EXTENDED`, `PERL`, `NOCASE`, `UTF8`, `INVERT`, `COUNT`, `NUMBER`, `NAMES`, `FILESONLY`, `ONLY`, and `QUIET`. The new `Regex` additionally accepts `DOTALL`, `MULTILINE`, `UNGREEDY`, `UCP`, `LITERAL`, `LIMIT=N`.

PCRE2 always uses PCRE-compatible regex syntax; `EXTENDED` and `PERL` are accepted as compatibility switches, but selecting `EXTENDED` does **not** force strict POSIX ERE semantics. For strict classic/POSIX behaviour, use `ARegex`. PCRE2 supports lookbehind, named captures, Unicode properties (with `UTF8 UCP`), lazy quantifiers, and other modern syntax; `ONLY` still prints the matching text, not individual capture groups. Pattern matching is per line. `DOTALL` allows dots to match newlines *inside an input record*, but the command still reads and searches a single line at a time. No multi-line input buffering or replacement mode is claimed.

Examples:

- `Regex "error|warning" RAM:log.txt NOCASE NUMBER`
- `Regex "(?<id>[0-9]+)" RAM:log.txt ONLY`
- `Regex "\\p{L}+" RAM:words.txt UTF8 UCP ONLY`
- `Regex "a.*?b" RAM:data.txt UNGREEDY ONLY LIMIT=100000`
- `ARegex "[0-9]+" RAM:data.txt EXTENDED ONLY`

Return codes: 0 if any line matches, 5 if nothing matches, 20 if a pattern, match, input or interrupt error occurs. Requires AmigaOS 3.x with 68040/68060 compatible CPU; classic `ARegex` additionally requires `LIBS:regex.library`. `Regex` currently embeds PCRE2's static engine; this is a **command migration, not yet a separately versioned PCRE2 shared Amiga library**. A shared PCRE2 ABI is future work and must not silently change existing `regex.library` LVOs.

Build with `PREFIX=/path/to/gcc16/prefix OUT=/path/to/output sh build-amiga.sh`. The PCRE2 builder checks the exact tagged source SHA and cross-compiles PCRE2 with Unicode enabled, JIT off. `BUILD_PCRE2=0` builds only the classic commands and library.
