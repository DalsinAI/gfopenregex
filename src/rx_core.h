/* GFOpenRegex core: regular expressions written from POSIX.1-2017
 * (XBD 9, regcomp/regexec) and the Perl syntax as perlre documents it.
 * No Amiga headers here: the same code builds the library, the host tests
 * and anything that links it statically. Memory comes only from rx_env.
 * MIT, Copyright (c) 2026 AmiCradle Team. */
#ifndef RX_CORE_H
#define RX_CORE_H

typedef unsigned char rx_u8;
typedef unsigned short rx_u16;
typedef unsigned int rx_u32;
typedef int rx_i32;
typedef long rx_off; /* regoff_t: libnix's off_t, a long */

struct rx_env {
	void *(*alloc)(struct rx_env *env, rx_u32 size);
	void (*free)(struct rx_env *env, void *mem);
	void *user;
};

/* regcomp flags (the values are libnix's and the BSDs' regex.h) */
#define RX_EXTENDED   0x0001
#define RX_ICASE      0x0002
#define RX_NOSUB      0x0004
#define RX_NEWLINE    0x0008
#define RX_NOSPEC     0x0010
#define RX_PEND       0x0020
/* GFOpenRegex's own */
#define RX_PERL       0x1000 /* Perl syntax, leftmost-first */
#define RX_UTF8       0x2000 /* pattern and subject are UTF-8 */
#define RX_DOTALL     0x4000 /* Perl: . matches newline, as (?s) */
#define RX_MULTILINE  0x8000 /* Perl: ^ $ at lines, as (?m) */
#define RX_FIRSTMATCH 0x10000 /* leftmost-first in POSIX syntax too */
#define RX_BACKTRACK  0x40000 /* tests: always the backtracker */

/* regexec flags */
#define RX_NOTBOL     0x0001
#define RX_NOTEOL     0x0002
#define RX_STARTEND   0x0004

/* errors (the values are libnix's regex.h) */
#define RX_OK        0
#define RX_NOMATCH   1
#define RX_BADPAT    2
#define RX_ECOLLATE  3
#define RX_ECTYPE    4
#define RX_EESCAPE   5
#define RX_ESUBREG   6
#define RX_EBRACK    7
#define RX_EPAREN    8
#define RX_EBRACE    9
#define RX_BADBR    10
#define RX_ERANGE   11
#define RX_ESPACE   12
#define RX_BADRPT   13
#define RX_EMPTY    14
#define RX_ASSERT   15
#define RX_INVARG   16
#define RX_ENOLIB   17 /* the link stub could not open regex.library */
#define RX_ILLSEQ   18 /* RX_UTF8 and the pattern is not UTF-8 */

struct rx_prog;

struct rx_match {
	rx_off so, eo;
};

#ifdef __cplusplus
extern "C" {
#endif

/* Compiles pattern[0..len). On success *out is the program and *nsub the
 * number of capturing groups. */
int rx_compile(struct rx_env *env, const char *pattern, rx_u32 len, rx_u32 cflags,
               struct rx_prog **out, rx_u32 *nsub);
/* Matches subject[start..end) (byte offsets in subject; end is the end of
 * the subject as $ sees it). Fills m[0..nmatch) with byte offsets in
 * subject, -1 for groups that took no part. */
int rx_exec(struct rx_env *env, const struct rx_prog *prog, const char *subject,
            rx_u32 start, rx_u32 end, rx_u32 nmatch, struct rx_match *m, rx_u32 eflags);
void rx_free(struct rx_env *env, struct rx_prog *prog);
/* POSIX regerror: the message for code, as much as fits (with a NUL) in
 * buf[0..size); returns the full length plus one. */
rx_u32 rx_error(int code, char *buf, rx_u32 size);
rx_u32 rx_strlen(const char *s);

#ifdef __cplusplus
}
#endif

#endif
