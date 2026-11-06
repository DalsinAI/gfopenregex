/* RegexTest: the vectors file (tests/vectors.txt: flags, pattern, subject
 * and the answer, tab separated, C escapes) through <regex.h> and
 * libregex.a, so through regex.library. Prints each case that differs and
 * a count. RegexTest [FILE] [BENCH n]. MIT, Copyright (c) 2026 AmiCradle
 * Team. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

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
			case 'x': { unsigned v = 0; int k; for (k = 1; k <= 2; k++) { char c = i[k];
				v = v * 16 + (unsigned)(c <= '9' ? c - '0' : (c | 32) - 'a' + 10); }
				*o++ = (char)v; i += 3; break; }
			default: *o++ = '\\'; break;
			}
		} else *o++ = *i++;
	}
	*o = 0;
	return (size_t)(o - s);
}

int main(int argc, char **argv)
{
	static char line[4096], got[512];
	const char *file = argc > 1 ? argv[1] : "vectors.txt";
	FILE *f = fopen(file, "r");
	long n = 0, bad = 0;
	if (!f) { printf("RegexTest: can't open %s\n", file); return 20; }
	while (fgets(line, sizeof line, f)) {
		char *fl = line, *pat, *sub, *exp, *nl, *o;
		size_t sl;
		int cf = 0, ef = 0, r, i;
		regex_t re;
		regmatch_t m[10];
		nl = strchr(line, '\n'); if (nl) *nl = 0;
		if (!line[0] || line[0] == '#') continue;
		pat = strchr(fl, '\t'); if (!pat) continue; *pat++ = 0;
		sub = strchr(pat, '\t'); if (!sub) continue; *sub++ = 0;
		exp = strchr(sub, '\t'); if (!exp) continue; *exp++ = 0;
		unesc(pat); sl = unesc(sub);
		for (; *fl; fl++) switch (*fl) {
		case 'E': cf |= REG_EXTENDED; break;
		case 'P': cf |= REG_PERL; break;
		case 'I': cf |= REG_ICASE; break;
		case 'N': cf |= REG_NEWLINE; break;
		case 'U': cf |= REG_UTF8; break;
		case 'S': cf |= REG_NOSUB; break;
		case 'F': cf |= REG_FIRSTMATCH; break;
		case 'M': cf |= REG_MULTILINE; break;
		case 'D': cf |= REG_DOTALL; break;
		case 'B': ef |= REG_NOTBOL; break;
		case 'L': ef |= REG_NOTEOL; break;
		}
		n++;
		o = got;
		r = regcomp(&re, pat, cf);
		if (r) sprintf(got, "ERR %d", r);
		else {
			m[0].rm_so = 0; m[0].rm_eo = (regoff_t)sl;
			r = regexec(&re, sub, 10, m, ef | REG_STARTEND);
			if (r == REG_NOMATCH) strcpy(got, "NOMATCH");
			else if (r) sprintf(got, "EXECERR %d", r);
			else
				for (i = 0; i < 10 && i <= (int)re.re_nsub; i++)
					o += sprintf(o, i ? " %ld,%ld" : "%ld,%ld", (long)m[i].rm_so, (long)m[i].rm_eo);
			regfree(&re);
		}
		if (strcmp(got, exp)) {
			bad++;
			if (bad <= 20) printf("DIFF line %ld: got %s, want %s\n", n, got, exp);
		}
	}
	fclose(f);
	printf("RegexTest: %ld cases, %ld differ\n", n, bad);
	return bad ? 10 : 0;
}
