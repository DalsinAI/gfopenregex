/* MIT: AmigaDOS C:Regex uses the new shared PCRE2 regex.library v2.
 * The PCRE2 core in that library remains BSD-3-Clause WITH PCRE2-exception. */
#ifndef GF_REGEX_PCRE2_COMPAT_H
#define GF_REGEX_PCRE2_COMPAT_H
#include <regex.h>
#define GFREG_MATCH_ERROR REG_BADPAT
#define GFREG_NOMATCH REG_NOMATCH
#define GFREG_COMPILE_ERROR REG_BADPAT
static int gfreg_match_limit(regex_t *re,unsigned long limit){return regsetmatchlimit(re,limit);}
#endif
