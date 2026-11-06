/* GFOpenRegex's calls on regex_t, the same in regex.library and in a
 * static link: regcomp and regexec with lengths, REG_PEND and
 * REG_STARTEND. MIT, Copyright (c) 2026 AmiCradle Team. */
#include <regex.h>
#include "rx_core.h"
#include "rx_api.h"

#define RX_MAGIC 0x52784731 /* "RxG1" */

long rx_api_ncomp(struct rx_env *env, regex_t *preg, const char *pattern,
                  unsigned long length, long cflags)
{
	struct rx_prog *p;
	rx_u32 nsub = 0;
	int r;
	if (!preg) return REG_INVARG;
	preg->re_magic = 0;
	preg->re_g = 0;
	r = rx_compile(env, pattern, (rx_u32)length, (rx_u32)cflags & ~(rx_u32)RX_BACKTRACK, &p, &nsub);
	if (r) return r;
	preg->re_magic = RX_MAGIC;
	preg->re_nsub = nsub;
	preg->re_g = (struct re_guts *)p;
	return 0;
}

long rx_api_comp(struct rx_env *env, regex_t *preg, const char *pattern, long cflags)
{
	unsigned long len;
	if (!preg || !pattern) return REG_INVARG;
	if (cflags & REG_PEND) {
		if (!preg->re_endp || preg->re_endp < pattern) return REG_INVARG;
		len = (unsigned long)(preg->re_endp - pattern);
	} else {
		len = rx_strlen(pattern);
	}
	return rx_api_ncomp(env, preg, pattern, len, cflags);
}

long rx_api_nexec(struct rx_env *env, const regex_t *preg, const char *string,
                  unsigned long length, unsigned long nmatch, regmatch_t *pmatch, long eflags)
{
	rx_u32 start = 0, end = (rx_u32)length;
	if (!preg || preg->re_magic != RX_MAGIC || !preg->re_g) return REG_BADPAT;
	if (!string) return REG_INVARG;
	if (eflags & REG_STARTEND) {
		if (!pmatch || pmatch[0].rm_so < 0 || pmatch[0].rm_eo < pmatch[0].rm_so)
			return REG_INVARG;
		start = (rx_u32)pmatch[0].rm_so;
		end = (rx_u32)pmatch[0].rm_eo;
	}
	/* regmatch_t is two longs, as struct rx_match */
	return rx_exec(env, (const struct rx_prog *)preg->re_g, string, start, end,
	               (rx_u32)nmatch, (struct rx_match *)pmatch, (rx_u32)eflags);
}

long rx_api_exec(struct rx_env *env, const regex_t *preg, const char *string,
                 unsigned long nmatch, regmatch_t *pmatch, long eflags)
{
	unsigned long len = 0;
	if (!string) return REG_INVARG;
	if (!(eflags & REG_STARTEND)) len = rx_strlen(string);
	return rx_api_nexec(env, preg, string, len, nmatch, pmatch, eflags);
}

unsigned long rx_api_error(long errcode, const regex_t *preg, char *buf, unsigned long size)
{
	(void)preg;
	return rx_error((int)errcode, buf, (rx_u32)size);
}

void rx_api_free(struct rx_env *env, regex_t *preg)
{
	if (!preg || preg->re_magic != RX_MAGIC) return;
	rx_free(env, (struct rx_prog *)preg->re_g);
	preg->re_g = 0;
	preg->re_magic = 0;
}
