/* Host tool: each stdin line "flags<TAB>pattern<TAB>subject" (C escapes
 * \n \t \\ \xHH in pattern and subject) gives one line: the regcomp error,
 * NOMATCH, or the match offsets "so,eo so,eo ...". flags: E extended,
 * P perl, I icase, N newline, U utf8, B notbol, L noteol, S nosub,
 * F firstmatch. With -glibc the same through the C library's regex.h
 * (E I N B L S only). MIT, Copyright (c) 2026 AmiCradle Team. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef USE_GLIBC
#include <regex.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
static sigjmp_buf jb;
static void on_alarm(int s) { (void)s; siglongjmp(jb, 1); }
#else
#include "../src/rx_core.h"
#endif

static void *a(struct rx_env *e, unsigned n) { (void)e; return malloc(n); }
static void f(struct rx_env *e, void *p) { (void)e; free(p); }

static size_t unesc(char *s)
{
	char *o = s, *i = s;
	while (*i) {
		if (*i == '\\' && i[1]) {
			i++;
			switch (*i) {
			case 'n': *o++ = '\n'; i++; break;
			case 't': *o++ = '\t'; i++; break;
			case '\\': *o++ = '\\'; i++; break;
			case 'x': { unsigned v; sscanf(i + 1, "%2x", &v); *o++ = (char)v; i += 3; break; }
			default: *o++ = '\\'; break;
			}
		} else *o++ = *i++;
	}
	*o = 0;
	return (size_t)(o - s);
}

int main(void)
{
	static char line[1 << 16];
#ifndef USE_GLIBC
	struct rx_env env = { a, f, 0 };
#endif
	while (fgets(line, sizeof line, stdin)) {
		char *fl = line, *pat, *sub, *nl;
		size_t pl, sl;
		unsigned cf = 0, ef = 0;
		int r, i;
		nl = strchr(line, '\n'); if (nl) *nl = 0;
		pat = strchr(fl, '\t'); if (!pat) continue; *pat++ = 0;
		sub = strchr(pat, '\t'); if (!sub) continue; *sub++ = 0;
		pl = unesc(pat); sl = unesc(sub);
		for (; *fl; fl++) switch (*fl) {
#ifdef USE_GLIBC
		case 'E': cf |= REG_EXTENDED; break;
		case 'I': cf |= REG_ICASE; break;
		case 'N': cf |= REG_NEWLINE; break;
		case 'S': cf |= REG_NOSUB; break;
		case 'B': ef |= REG_NOTBOL; break;
		case 'L': ef |= REG_NOTEOL; break;
#else
		case 'E': cf |= RX_EXTENDED; break;
		case 'P': cf |= RX_PERL; break;
		case 'I': cf |= RX_ICASE; break;
		case 'N': cf |= RX_NEWLINE; break;
		case 'U': cf |= RX_UTF8; break;
		case 'S': cf |= RX_NOSUB; break;
		case 'F': cf |= RX_FIRSTMATCH; break;
		case 'T': cf |= RX_BACKTRACK; break;
		case 'M': cf |= RX_MULTILINE; break;
		case 'D': cf |= RX_DOTALL; break;
		case 'B': ef |= RX_NOTBOL; break;
		case 'L': ef |= RX_NOTEOL; break;
#endif
		}
#ifdef USE_GLIBC
		{
			regex_t re; regmatch_t m[10];
			(void)pl;
			r = regcomp(&re, pat, cf);
			if (r) { printf("ERR %d\n", r); fflush(stdout); continue; }
			signal(SIGALRM, on_alarm);
			if (sigsetjmp(jb, 1)) { printf("TIMEOUT\n"); fflush(stdout); continue; }
			alarm(1);
			m[0].rm_so = 0; m[0].rm_eo = (regoff_t)sl;
			r = regexec(&re, sub, 10, m, ef | REG_STARTEND);
			if (r) printf("NOMATCH\n");
			else {
				for (i = 0; i < 10 && i <= (int)re.re_nsub; i++)
					printf(i ? " %d,%d" : "%d,%d", (int)m[i].rm_so, (int)m[i].rm_eo);
				printf("\n");
			}
			alarm(0);
			regfree(&re);
		}
#else
		{
			struct rx_prog *p; unsigned nsub; struct rx_match m[10];
			r = rx_compile(&env, pat, (unsigned)pl, cf, &p, &nsub);
			if (r) { printf("ERR %d\n", r); fflush(stdout); continue; }
			r = rx_exec(&env, p, sub, 0, (unsigned)sl, 10, m, ef);
			if (r == 1) printf("NOMATCH\n");
			else if (r) printf("EXECERR %d\n", r);
			else {
				for (i = 0; i < 10 && i <= (int)nsub; i++)
					printf(i ? " %ld,%ld" : "%ld,%ld", (long)m[i].so, (long)m[i].eo);
				printf("\n");
			}
			rx_free(&env, p);
		}
#endif
		fflush(stdout);
	}
	return 0;
}
