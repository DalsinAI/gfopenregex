# GFOpenRegex

`regex.library`: regular expressions for AmigaOS 3.x, as a shared library
and a link stub. POSIX `regcomp`/`regexec`/`regerror`/`regfree` (basic and
extended syntax), with Perl syntax, UTF-8 and lengths as options. One copy
in `LIBS:` for every port, in place of each port carrying its own regex
code (Boost.Regex, Henry Spencer's, glibc's...).

Clean room: written only from POSIX.1-2017 (XBD chapter 9, Regular
Expressions, and the regcomp/regexec page) and Perl's perlre
documentation. No code from another regex implementation (glibc, the
BSDs' or Henry Spencer's, PCRE, RE2, TRE, Oniguruma, Boost.Regex or
libstdc++'s std::regex) was used. Python's `re` and glibc's `regcomp` are
used only as test references, run on the PC.

By the AmiCradle Team. MIT licensed (LICENSE).

| Path | What it is |
| --- | --- |
| `src/rx_compile.c` | pattern to program, no recursion (frames on the heap) |
| `src/rx_exec.c` | the Pike VM and the heap backtracker |
| `src/rx_unicode.c` | UTF-8, case and classes (ISO 8859-1 bytes; code points by script blocks) |
| `src/rx_api.c` | the calls on `regex_t` |
| `library/rx_lib.c` | regex.library: RomTag, jump table, AllocVec |
| `stub/regex_stub.c` | libregex.a: the POSIX calls, opening the library |
| `command/regex_cmd.c` | `C:Regex`, a grep for AmigaDOS on regex.library |
| `package/` | `Install-GFOpenRegex`, the Amiga Installer script, and its icon |
| `include/` | `regex.h`, `libraries/regex.h`, `proto/`, `clib/`, `inline/`, `fd/`, `sfd/regex_lib.sfd` |
| `docs/regex.library.md` | the calls, flags, syntax and limits |
| `tests/` | hand-written cases, reference vectors, the PC tools, RegexTest |

## Building

    STOVE=~/AmiCradle/stoves/os32-gcc16 sh build-amiga.sh

or with a GFOpenAmigaGCC release and OpenNDK's headers:

    PREFIX=/path/to/gfopenamigagcc-16.2.0-open6 NDK_INC=/path/to/OpenNDK/Include_H sh build-amiga.sh

gives `build-amiga/lib/regex.library` (copy to `LIBS:`),
`build-amiga/lib/libregex.a`, `build-amiga/c/Regex`, `include/`, `docs/`
and `tests/RegexTest`. `sh tools/package.sh 1.0-rc1` packs them with
Install-GFOpenRegex as `dist/GFOpenRegex-1.0-rc1.lha` and `.zip`. The build runs the PC tests first when the PC has a C
compiler and checks that neither the library nor the stub uses floating
point (68040, soft float specs; nothing in them needs an FPU).

`include/sfd/regex_lib.sfd` is the source of the fd, clib and inline
headers: after changing it run `python3 tools/gen_headers.py`.

## Using it

    #include <regex.h>
    regex_t re;
    regmatch_t m[2];
    if (regcomp(&re, "([a-z]+)@example\\.com", REG_EXTENDED | REG_ICASE) == 0) {
        if (regexec(&re, line, 2, m, 0) == 0)
            printf("user %.*s\n", (int)(m[1].rm_eo - m[1].rm_so), line + m[1].rm_so);
        regfree(&re);
    }

and link with `-lregex`. `REG_PERL` gives Perl syntax (`\d`, `(?:...)`,
lazy `*?`, `(?=...)`); `REG_UTF8` makes `.` and classes take whole UTF-8
characters.

## Tests

- `sh tests/run_host_tests.sh`: the core on the PC (AddressSanitizer)
  against `tests/cases.txt` (106 hand-written answers) and
  `tests/vectors.txt` (1,041 random cases with the references' answers).
  `FUZZ=3000` adds new random cases against Python's `re` (Perl syntax:
  every group) and glibc (POSIX: the whole match).
- `RegexTest tests/all.txt` on the Amiga (or vamos, `-C 68040`): the same
  cases through `<regex.h>`, libregex.a and regex.library.

Where glibc and GFOpenRegex differ, it has been glibc's back-reference
handling (it misses matches such as `\(B\{0,1\}\)...\1` on an empty
group); those cases are left out of the vectors.
