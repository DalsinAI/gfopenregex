/* Regex: search files for lines that match a regular expression, through
 * regex.library. The AmigaDOS command of GFOpenRegex.
 *
 * Template: PATTERN/A,FILES/M,EXTENDED/S,PERL/S,NOCASE/S,UTF8/S,INVERT/S,
 *           COUNT/S,NUMBER/S,NAMES/S,FILESONLY/S,ONLY/S,QUIET/S
 * (no one-letter keywords, so a short pattern isn't taken for one)
 * FILES takes AmigaDOS wildcards (#?.c); no FILES reads the input.
 * Return code 0 when a line matched, 5 (WARN) when none did, 20 on an
 * error (a bad pattern, a file that can't be read, no regex.library).
 * MIT, Copyright (c) 2026 AmiCradle Team. */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosasl.h>
#include <dos/rdargs.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <regex.h>
#include <stdio.h>
#include <string.h>

static const char version[] __attribute__((used)) =
	"$VER: Regex 1.0 (6.11.2026) GFOpenRegex, AmiCradle Team";

#define TEMPLATE "PATTERN/A,FILES/M,EXTENDED/S,PERL/S,NOCASE/S,UTF8/S," \
                 "INVERT/S,COUNT/S,NUMBER/S,NAMES/S,FILESONLY/S,ONLY/S,QUIET/S"
enum { A_PATTERN, A_FILES, A_EXT, A_PERL, A_NOCASE, A_UTF8, A_INVERT, A_COUNT,
       A_NUMBER, A_NAMES, A_FILESONLY, A_ONLY, A_QUIET, A_N };

static LONG args[A_N];
static regex_t re;
static char *line;
static size_t cap = 4096;
static long total;

/* one line of any length into line[]; returns its length or -1 at the end */
static long read_line(BPTR fh)
{
	size_t n = 0;
	LONG c;
	for (;;) {
		c = FGetC(fh);
		if (c < 0) break;
		if (n + 2 >= cap) {
			char *b = (char *)AllocVec(cap * 2, MEMF_ANY);
			if (!b) break;
			CopyMem(line, b, n);
			FreeVec(line);
			line = b;
			cap *= 2;
		}
		if (c == '\n') break;
		line[n++] = (char)c;
	}
	if (c < 0 && n == 0) return -1;
	line[n] = 0;
	return (long)n;
}

/* searches one file; returns 1 when a line matched, 0 when none, -1 on a
 * read error. name is NULL for the input. */
static int search(BPTR fh, const char *name, int names)
{
	long n, lineno = 0, count = 0;
	regmatch_t m;
	while ((n = read_line(fh)) >= 0) {
		int hit;
		lineno++;
		if (SetSignal(0, 0) & SIGBREAKF_CTRL_C) {
			SetSignal(0, SIGBREAKF_CTRL_C);
			PutStr("***Break\n");
			return -2;
		}
		m.rm_so = 0;
		m.rm_eo = n;
		hit = regexec(&re, line, 1, &m, REG_STARTEND) == 0;
		if (args[A_INVERT]) hit = !hit;
		if (!hit) continue;
		count++;
		if (args[A_QUIET] || args[A_COUNT]) continue;
		if (args[A_FILESONLY]) break;
		if (args[A_ONLY] && !args[A_INVERT]) {
			/* every match in the line, one per output line */
			long pos = 0;
			while (pos <= n) {
				m.rm_so = pos;
				m.rm_eo = n;
				if (regexec(&re, line, 1, &m, REG_STARTEND | (pos ? REG_NOTBOL : 0)))
					break;
				if (m.rm_eo > m.rm_so) {
					if (names && name) printf("%s:", name);
					if (args[A_NUMBER]) printf("%ld:", lineno);
					printf("%.*s\n", (int)(m.rm_eo - m.rm_so), line + m.rm_so);
				}
				pos = m.rm_eo > m.rm_so ? m.rm_eo : m.rm_eo + 1;
			}
		} else {
			if (names && name) printf("%s:", name);
			if (args[A_NUMBER]) printf("%ld:", lineno);
			fwrite(line, 1, (size_t)n, stdout);
			putchar('\n');
		}
	}
	total += count;
	if (args[A_COUNT] && !args[A_QUIET]) {
		if (names && name) printf("%s:", name);
		printf("%ld\n", count);
	}
	if (args[A_FILESONLY] && count && !args[A_QUIET] && name) printf("%s\n", name);
	return count ? 1 : 0;
}

int main(void)
{
	struct RDArgs *rd;
	int rc = RETURN_WARN, cflags = 0, r, any = 0;
	char err[128];
	rd = ReadArgs((CONST_STRPTR)TEMPLATE, args, NULL);
	if (!rd) {
		PrintFault(IoErr(), (CONST_STRPTR)"Regex");
		return RETURN_ERROR;
	}
	if (args[A_PERL]) cflags |= REG_PERL;
	else if (args[A_EXT]) cflags |= REG_EXTENDED;
	if (args[A_NOCASE]) cflags |= REG_ICASE;
	if (args[A_UTF8]) cflags |= REG_UTF8;
	if (args[A_INVERT] || args[A_COUNT] || args[A_QUIET] || args[A_FILESONLY]) cflags |= REG_NOSUB;
	r = regcomp(&re, (const char *)args[A_PATTERN], cflags);
	if (r) {
		regerror(r, &re, err, sizeof err);
		printf("Regex: %s\n", err);
		FreeArgs(rd);
		return RETURN_FAIL;
	}
	line = (char *)AllocVec(cap, MEMF_ANY);
	if (!line) { PutStr("Regex: not enough memory\n"); regfree(&re); FreeArgs(rd); return RETURN_FAIL; }
	if (!args[A_FILES]) {
		fflush(stdout);
		r = search(Input(), NULL, 0);
		if (r > 0) any = 1;
		if (r == -2) rc = RETURN_FAIL;
	} else {
		STRPTR *f;
		struct AnchorPath *ap = (struct AnchorPath *)AllocVec(sizeof(struct AnchorPath) + 512, MEMF_CLEAR);
		int multi = 0, n = 0;
		for (f = (STRPTR *)args[A_FILES]; *f; f++) n++;
		multi = n > 1 || args[A_NAMES];
		if (!ap) { PutStr("Regex: not enough memory\n"); rc = RETURN_FAIL; goto done; }
		ap->ap_Strlen = 512;
		for (f = (STRPTR *)args[A_FILES]; *f && rc != RETURN_FAIL; f++) {
			LONG e;
			int wild = ParsePattern(*f, (STRPTR)err, sizeof err) == 1;
			if (wild) multi = 1;
			for (e = MatchFirst(*f, ap); e == 0; e = MatchNext(ap)) {
				BPTR fh;
				if (ap->ap_Info.fib_DirEntryType > 0) continue;
				fh = Open(ap->ap_Buf, MODE_OLDFILE);
				if (!fh) {
					printf("Regex: can't open %s\n", (char *)ap->ap_Buf);
					rc = RETURN_FAIL;
					continue;
				}
				fflush(stdout);
				r = search(fh, (const char *)ap->ap_Buf, multi);
				Close(fh);
				if (r > 0) any = 1;
				if (r == -2) { rc = RETURN_FAIL; break; }
			}
			if (e != ERROR_NO_MORE_ENTRIES && e != 0 && rc != RETURN_FAIL) {
				PrintFault(e, *f);
				rc = RETURN_FAIL;
			}
			MatchEnd(ap);
		}
		FreeVec(ap);
	}
done:
	fflush(stdout);
	if (rc != RETURN_FAIL) rc = any ? RETURN_OK : RETURN_WARN;
	FreeVec(line);
	regfree(&re);
	FreeArgs(rd);
	return rc;
}
