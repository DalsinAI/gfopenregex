/* Host bench: each pattern over a text, whole-text scan for every match
 * (regexec with REG_STARTEND, as grep would), ours against the C
 * library's regex. bench FILE. MIT, Copyright (c) 2026 AmiCradle Team. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <regex.h>
#include "../src/rx_core.h"

static void *a(struct rx_env *e, unsigned n) { (void)e; return malloc(n); }
static void f(struct rx_env *e, void *p) { (void)e; free(p); }
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }

int main(int argc, char **argv)
{
	static const struct { const char *p; int ere; } pats[] = {
		{ "the", 1 }, { "[A-Z][a-z]+ [A-Z][a-z]+", 1 }, { "[0-9]+\\.[0-9]+", 1 },
		{ "(foo|bar|baz|qux)[a-z]*", 1 }, { "^[^ ]+ [^ ]+$", 1 }, { "\\<w[a-z]*\\>", 1 },
		{ "([a-z]+)@([a-z]+)\\.com", 1 }, { "x*y*z*q", 1 },
	};
	struct rx_env env = { a, f, 0 };
	FILE *fp = fopen(argv[1], "rb");
	char *text; long n; unsigned i;
	fseek(fp, 0, SEEK_END); n = ftell(fp); fseek(fp, 0, SEEK_SET);
	text = malloc(n + 1); fread(text, 1, n, fp); text[n] = 0;
	printf("| pattern | GFOpenRegex | glibc regex (native) | matches |\n|---|---|---|---|\n");
	for (i = 0; i < sizeof pats / sizeof pats[0]; i++) {
		struct rx_prog *p; unsigned ns; struct rx_match m[3];
		regex_t re; regmatch_t g[3];
		long c1 = 0, c2 = 0; unsigned pos = 0; double t0, t1, t2;
		int fl = REG_EXTENDED | REG_NEWLINE;
		rx_compile(&env, pats[i].p, strlen(pats[i].p), RX_EXTENDED | RX_NEWLINE, &p, &ns);
		t0 = now();
		while (pos <= (unsigned)n && rx_exec(&env, p, text, pos, n, 3, m, pos ? RX_NOTBOL : 0) == 0) {
			c1++; pos = m[0].eo > m[0].so ? m[0].eo : m[0].eo + 1;
		}
		t1 = now();
		regcomp(&re, pats[i].p, fl);
		pos = 0;
		for (;;) {
			g[0].rm_so = pos; g[0].rm_eo = n;
			if (pos > (unsigned)n || regexec(&re, text, 3, g, REG_STARTEND | (pos ? REG_NOTBOL : 0))) break;
			c2++; pos = g[0].rm_eo > g[0].rm_so ? g[0].rm_eo : g[0].rm_eo + 1;
		}
		t2 = now();
		printf("| `%s` | %.3f s | %.3f s | %ld%s |\n", pats[i].p, t1 - t0, t2 - t1, c1, c1 == c2 ? "" : " (glibc differs)");
		rx_free(&env, p); regfree(&re);
	}
	return 0;
}
