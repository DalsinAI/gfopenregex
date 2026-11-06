/* regex.h: POSIX regular expressions for AmigaOS 3.x, served by
 * regex.library (GFOpenRegex) through the libregex.a link stub.
 * The layout and the flag values are libnix's <regex.h> (the BSDs'), so a
 * program built against either header links with -lregex.
 * MIT, Copyright (c) 2026 AmiCradle Team. */
#ifndef _REGEX_H_
#define _REGEX_H_

#include <stddef.h>

/* libnix's off_t, a long; only <stddef.h> here, so the header also builds
 * with -nostdinc (the Open SDK's stubs) */
typedef long regoff_t;

typedef struct {
	int re_magic;
	size_t re_nsub;          /* the number of ( ) groups */
	const char *re_endp;     /* the pattern's end, with REG_PEND */
	struct re_guts *re_g;    /* regex.library's program */
} regex_t;

typedef struct {
	regoff_t rm_so;          /* start of the match, -1 for none */
	regoff_t rm_eo;          /* end of the match */
} regmatch_t;

/* regcomp() flags */
#define REG_BASIC      0000
#define REG_EXTENDED   0001
#define REG_ICASE      0002
#define REG_NOSUB      0004
#define REG_NEWLINE    0010
#define REG_NOSPEC     0020
#define REG_PEND       0040
/* GFOpenRegex's own */
#define REG_PERL       0x1000  /* Perl syntax, leftmost-first */
#define REG_UTF8       0x2000  /* pattern and subject are UTF-8 */
#define REG_DOTALL     0x4000  /* with REG_PERL: . takes a newline too, as (?s) */
#define REG_MULTILINE  0x8000  /* with REG_PERL: ^ and $ at every line, as (?m) */
#define REG_FIRSTMATCH 0x10000 /* leftmost-first in POSIX syntax too */

/* errors */
#define REG_NOMATCH     1
#define REG_BADPAT      2
#define REG_ECOLLATE    3
#define REG_ECTYPE      4
#define REG_EESCAPE     5
#define REG_ESUBREG     6
#define REG_EBRACK      7
#define REG_EPAREN      8
#define REG_EBRACE      9
#define REG_BADBR      10
#define REG_ERANGE     11
#define REG_ESPACE     12
#define REG_BADRPT     13
#define REG_EMPTY      14
#define REG_ASSERT     15
#define REG_INVARG     16
#define REG_ENOLIB     17  /* libregex.a could not open regex.library */
#define REG_ILLSEQ     18

/* regexec() flags */
#define REG_NOTBOL     00001
#define REG_NOTEOL     00002
#define REG_STARTEND   00004

#ifdef __cplusplus
extern "C" {
#endif
int regcomp(regex_t *preg, const char *pattern, int cflags);
int regexec(const regex_t *preg, const char *string, size_t nmatch,
            regmatch_t pmatch[], int eflags);
size_t regerror(int errcode, const regex_t *preg, char *errbuf, size_t errbuf_size);
void regfree(regex_t *preg);
/* GFOpenRegex's own: pattern and subject with lengths (NULs allowed) */
int regncomp(regex_t *preg, const char *pattern, size_t length, int cflags);
int regnexec(const regex_t *preg, const char *string, size_t length, size_t nmatch,
             regmatch_t pmatch[], int eflags);
#ifdef __cplusplus
}
#endif

#endif
