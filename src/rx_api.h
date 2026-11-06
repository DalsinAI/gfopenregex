/* GFOpenRegex's regex_t calls (src/rx_api.c). MIT, Copyright (c) 2026
 * AmiCradle Team. */
#ifndef RX_API_H
#define RX_API_H
long rx_api_comp(struct rx_env *env, regex_t *preg, const char *pattern, long cflags);
long rx_api_ncomp(struct rx_env *env, regex_t *preg, const char *pattern,
                  unsigned long length, long cflags);
long rx_api_exec(struct rx_env *env, const regex_t *preg, const char *string,
                 unsigned long nmatch, regmatch_t *pmatch, long eflags);
long rx_api_nexec(struct rx_env *env, const regex_t *preg, const char *string,
                  unsigned long length, unsigned long nmatch, regmatch_t *pmatch, long eflags);
unsigned long rx_api_error(long errcode, const regex_t *preg, char *buf, unsigned long size);
void rx_api_free(struct rx_env *env, regex_t *preg);
#endif
