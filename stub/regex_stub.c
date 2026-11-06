/* libregex.a: the POSIX calls of <regex.h>, served by regex.library.
 * Link with -lregex; the library opens at the first call and closes when
 * the program ends. A program that can't open it gets REG_ENOLIB from
 * regcomp, and regerror says so. MIT, Copyright (c) 2026 AmiCradle Team. */
#include <exec/types.h>
#include <proto/exec.h>
#include <stabs.h>
#include <libraries/regex.h>
#define REGEX_BASE_NAME __regex_base
#include <inline/regex.h>

struct Library *__regex_base;
static int tried;

static int rx_open(void)
{
	if (!__regex_base && !tried) {
		tried = 1;
		__regex_base = OpenLibrary((CONST_STRPTR)REGEXNAME, REGEX_VERSION);
	}
	return __regex_base != 0;
}

static void __attribute__((used)) rx_close(void)
{
	if (__regex_base) {
		CloseLibrary(__regex_base);
		__regex_base = 0;
	}
}
ADD2EXIT(rx_close, -50);

int regcomp(regex_t *preg, const char *pattern, int cflags)
{
	if (!rx_open()) return REG_ENOLIB;
	return (int)RegComp(preg, (CONST_STRPTR)pattern, cflags);
}

int regexec(const regex_t *preg, const char *string, size_t nmatch, regmatch_t pmatch[], int eflags)
{
	if (!rx_open()) return REG_ENOLIB;
	return (int)RegExec(preg, (CONST_STRPTR)string, nmatch, pmatch, eflags);
}

size_t regerror(int errcode, const regex_t *preg, char *errbuf, size_t errbuf_size)
{
	static const char nolib[] = "regex.library 1 could not be opened";
	if (!rx_open() || errcode == REG_ENOLIB) {
		size_t n = sizeof nolib, k;
		if (errbuf && errbuf_size) {
			k = n <= errbuf_size ? n : errbuf_size;
			CopyMem((APTR)nolib, errbuf, k);
			errbuf[k - 1] = 0;
		}
		return n;
	}
	return RegError(errcode, preg, (STRPTR)errbuf, errbuf_size);
}

void regfree(regex_t *preg)
{
	if (__regex_base) RegFree(preg);
}

/* GFOpenRegex's own: pattern and subject with lengths (NULs allowed) */
int regncomp(regex_t *preg, const char *pattern, size_t length, int cflags)
{
	if (!rx_open()) return REG_ENOLIB;
	return (int)RegNComp(preg, (CONST_STRPTR)pattern, length, cflags);
}

int regnexec(const regex_t *preg, const char *string, size_t length, size_t nmatch,
             regmatch_t pmatch[], int eflags)
{
	if (!rx_open()) return REG_ENOLIB;
	return (int)RegNExec(preg, (CONST_STRPTR)string, length, nmatch, pmatch, eflags);
}
