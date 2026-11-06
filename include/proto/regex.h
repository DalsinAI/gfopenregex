/* regex.library's proto header. MIT, Copyright (c) 2026 AmiCradle Team.
 * Most programs want <regex.h> and -lregex instead: the link stub opens
 * the library and keeps the POSIX calls. */
#ifndef PROTO_REGEX_H
#define PROTO_REGEX_H

#include <exec/types.h>
#include <libraries/regex.h>

#ifndef __NOLIBBASE__
extern struct Library *RegexBase;
#endif
#if defined(__GNUC__) && !defined(REGEX_NO_INLINE)
#include <inline/regex.h>
#else
#include <clib/regex_protos.h>
#endif

#endif
