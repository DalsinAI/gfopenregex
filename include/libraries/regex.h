/* regex.library (GFOpenRegex): the types and flags. They are <regex.h>'s:
 * see that header and docs/regex.library.md. MIT, Copyright (c) 2026
 * AmiCradle Team. */
#ifndef LIBRARIES_REGEX_H
#define LIBRARIES_REGEX_H

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif
#include <regex.h>

#define REGEXNAME "regex.library"
#define REGEX_VERSION 1

/* when libnix's <regex.h> came first */
#ifndef REG_PERL
#define REG_PERL       0x1000
#define REG_UTF8       0x2000
#define REG_DOTALL     0x4000
#define REG_MULTILINE  0x8000
#define REG_FIRSTMATCH 0x10000
#define REG_ENOLIB     17
#define REG_ILLSEQ     18
#endif

#endif
