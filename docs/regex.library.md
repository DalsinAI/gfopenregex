# regex.library 1.0

GFOpenRegex: POSIX regular expressions for AmigaOS 3.x (68020 and up,
built for the 68040), with Perl syntax as an option. No floating point, no
C library inside, about 20 KB.

## Two ways in

**The POSIX calls (most programs).** Include `<regex.h>` and link
`-lregex`. `libregex.a` is a link stub: `regcomp`, `regexec`, `regerror`
and `regfree` call regex.library, which opens at the first call and
closes when the program ends. A port that already calls `regcomp` needs no
source change. If the library can't be opened, `regcomp` and `regexec`
return `REG_ENOLIB` (17) and `regerror` says so.

`<regex.h>` has libnix's layout and flag values (the BSDs'), so a program
built against libnix's own `<regex.h>` links with `-lregex` too.

**The library calls.** `#include <proto/regex.h>`, open
`REGEXNAME` ("regex.library") version 1 into `RegexBase`:

| LVO | Call | Registers |
| --- | --- | --- |
| -30 | `LONG RegComp(regex_t *preg, CONST_STRPTR pattern, LONG cflags)` | a0, a1, d0 |
| -36 | `LONG RegExec(CONST regex_t *preg, CONST_STRPTR string, ULONG nmatch, regmatch_t *pmatch, LONG eflags)` | a0, a1, d0, a2, d1 |
| -42 | `ULONG RegError(LONG errcode, CONST regex_t *preg, STRPTR errbuf, ULONG size)` | d0, a0, a1, d1 |
| -48 | `VOID RegFree(regex_t *preg)` | a0 |
| -54 | `LONG RegNComp(regex_t *preg, CONST_STRPTR pattern, ULONG length, LONG cflags)` | a0, a1, d0, d1 |
| -60 | `LONG RegNExec(CONST regex_t *preg, CONST_STRPTR string, ULONG length, ULONG nmatch, regmatch_t *pmatch, LONG eflags)` | a0, a1, d0, d1, a2, d2 |

They are `regcomp`, `regexec`, `regerror` and `regfree`, plus two that
take lengths, so patterns and subjects may hold NUL bytes (`regncomp` and
`regnexec` in the stub). `include/sfd/regex_lib.sfd` is the source of the
fd, clib and inline headers.

## Flags

`regcomp`: `REG_EXTENDED` (else basic), `REG_ICASE`, `REG_NOSUB`,
`REG_NEWLINE`, `REG_NOSPEC` (the whole pattern is literal), `REG_PEND`
(the pattern ends at `preg->re_endp`). GFOpenRegex's own:

| Flag | Value | Meaning |
| --- | --- | --- |
| `REG_PERL` | 0x1000 | Perl syntax and Perl's leftmost-first matching |
| `REG_UTF8` | 0x2000 | the pattern and the subject are UTF-8: `.` and classes take one character, offsets stay in bytes |
| `REG_DOTALL` | 0x4000 | with `REG_PERL`: `.` takes a newline too, as `(?s)` |
| `REG_MULTILINE` | 0x8000 | with `REG_PERL`: `^` and `$` at every line, as `(?m)` |
| `REG_FIRSTMATCH` | 0x10000 | leftmost-first in POSIX syntax too |

`regexec`: `REG_NOTBOL`, `REG_NOTEOL`, `REG_STARTEND` (search
`string[pmatch[0].rm_so .. pmatch[0].rm_eo)`; the offsets that come back
count from `string`, as the BSDs').

## Syntax

**POSIX basic and extended** (POSIX.1-2017 XBD 9): brackets with ranges,
`[:class:]`, `[=c=]`, `[.c.]`; `*`, `\{m,n\}` / `{m,n}`, groups,
back-references `\1`..`\9` (in extended syntax too); alternation. Also the
extensions programs written for glibc use: `\|`, `\+`, `\?` in basic
syntax, `{,n}`, `\w \W \s \S \b \B \< \> \` \'`. Matching is POSIX's
leftmost-longest for the whole match; groups take the first way found
that gives it (as most implementations, not the full POSIX sub-expression
rule).

**Perl** (`REG_PERL`, perlre): `\d \D \w \W \s \S \b \B \A \z \Z`,
`\n \t \r \f \v \e \a \0oo \xHH \x{HHHH} \cX`, `\Q...\E`; lazy
quantifiers `*? +? ?? {m,n}?`; `(?:...)`, named groups `(?<n>...)`
`(?P<n>...)` `(?'n'...)` (numbered as usual; the names are not kept);
look-ahead `(?=...)` and `(?!...)`; back-references `\1`..`\9`, `\g{n}`;
inline flags `(?imsx-imsx)` and `(?imsx-imsx:...)`; comments `(?#...)`.
Not offered: look-behind (`REG_BADPAT`), possessive quantifiers and atomic
groups, recursion, Unicode properties `\p{..}`.

## Characters

Without `REG_UTF8` a character is a byte in ISO 8859-1, the Amiga's own
set: `REG_ICASE` and `[:alpha:]`, `[:upper:]`, `\w` etc. know its letters
(`é` and `É` are one letter). With `REG_UTF8` code points past 255 are
cased by the regular pairings of Latin, Greek, Cyrillic, Armenian and
fullwidth Latin, and classed by script blocks (letters of the main scripts
and CJK are `[:alpha:]` and `\w`); invalid bytes are characters of their
own. unicode.library is not needed.

## How it matches

Patterns without back-references or look-aheads run on a Pike VM: each
position of the subject once, time in proportion to subject times
pattern, so no pattern can take exponential time and no input uses more
stack. The rest run on a backtracker whose choice points are on the heap
(never the caller's stack), stopped after 20 million steps with
`REG_ESPACE`. In Perl syntax, patterns with loops that can match empty run
on the backtracker with a table of states tried (still linear), so the
empty-iteration rule is Perl's; when that table would pass 2 Mbit
(256 KB) they run on the Pike VM, whose groups can then differ from Perl's
in loops whose last pass took nothing.

The compiler doesn't recurse either. All memory comes from AllocVec; a
compiled `regex_t` is read-only to `regexec`, so tasks may share one.
Limits: 32767 instructions per program (`REG_ESPACE` past that), counts up
to 255.

## The Regex command

`C:Regex` searches files for lines that match, as grep does:

    Regex PATTERN/A,FILES/M,EXTENDED/S,PERL/S,NOCASE/S,UTF8/S,INVERT/S,
          COUNT/S,NUMBER/S,NAMES/S,FILESONLY/S,ONLY/S,QUIET/S

PATTERN is POSIX basic syntax, or extended with EXTENDED, or Perl's with
PERL (quote a pattern that looks like a keyword or holds spaces). FILES
take AmigaDOS wildcards (`#?.c`); with none, Regex reads its input.
INVERT shows lines that don't match, COUNT the number of lines, NUMBER
puts line numbers first, NAMES the file name (on by default for more than
one file), FILESONLY just the names of files with a match, ONLY just the
matching parts, QUIET nothing. Return code 0 when a line matched, 5 when
none did, 20 on an error. Ctrl-C stops it.

## Errors

`REG_NOMATCH` 1, `REG_BADPAT` 2, `REG_ECOLLATE` 3, `REG_ECTYPE` 4,
`REG_EESCAPE` 5, `REG_ESUBREG` 6, `REG_EBRACK` 7, `REG_EPAREN` 8,
`REG_EBRACE` 9, `REG_BADBR` 10, `REG_ERANGE` 11, `REG_ESPACE` 12,
`REG_BADRPT` 13, `REG_INVARG` 16, `REG_ENOLIB` 17. `regerror` gives the
text.
