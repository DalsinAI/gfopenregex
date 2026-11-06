GFOpenRegex 1.0, release candidate 1.

regex.library 1.0 for AmigaOS 3.x (built for the 68040, no floating
point), libregex.a, the link stub that keeps the POSIX calls of
`<regex.h>`, and C:Regex, a grep on the library. POSIX basic and
extended syntax, Perl syntax with `REG_PERL`, UTF-8 with `REG_UTF8`.
Clean room, MIT licence.

Install: double-click Install-GFOpenRegex. It copies regex.library to
LIBS: and Regex to C:, and the SDK and documents where you choose. Or
copy `GFOpenRegex/Libs/regex.library` to `LIBS:` by hand.

Check: in `GFOpenRegex/Tests`, `RegexTest regex-cases.txt` should end
with `0 differ`. Programs: include `<regex.h>` from `SDK/Include` and
link `SDK/Lib/libregex.a`. The calls, flags, syntax and the Regex
command are in `Docs/regex.library.md`.

Not yet run on a real 68040 (tested under vamos, a 68040 emulation).
