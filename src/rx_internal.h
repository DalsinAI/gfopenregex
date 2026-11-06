/* GFOpenRegex internals: the compiled program and the helpers the compiler
 * and the two matchers share. MIT, Copyright (c) 2026 AmiCradle Team. */
#ifndef RX_INTERNAL_H
#define RX_INTERNAL_H

#include "rx_core.h"

/* Instructions. Every consuming instruction takes one character: a byte,
 * or with RX_UTF8 one code point (an invalid byte is one character too). */
enum {
	OP_BYTE,    /* x: the byte */
	OP_BYTE2,   /* x or y: a byte and its other case */
	OP_CHAR,    /* RX_UTF8: x the code point */
	OP_CHARI,   /* RX_UTF8: x the folded code point, any case */
	OP_ANY,     /* arg 1: not newline */
	OP_CLASS,   /* x: index in classes */
	OP_MATCH,
	OP_SPLIT,   /* go to x, then (on failure) to y */
	OP_JMP,     /* x */
	OP_SAVE,    /* x: capture slot */
	OP_MARK,    /* x: loop mark, the position at a nullable body's start */
	OP_CHK,     /* x: loop mark; to y (the loop's exit) when the body took nothing */
	OP_ASSERT,  /* arg: AS_ */
	OP_BACKREF, /* x: group, arg 1: any case */
	OP_LOOK,    /* arg 1: negative; x: the instruction after its OP_LOOKEND */
	OP_LOOKEND
};

enum {
	AS_BOL,      /* at the start (not with RX_NOTBOL) */
	AS_BOLM,     /* at the start or after a newline */
	AS_EOL,      /* at the end (not with RX_NOTEOL) */
	AS_EOLM,     /* at the end or before a newline */
	AS_EOLP,     /* Perl $ and \Z: the end, or before a newline that ends it */
	AS_BOT,      /* \A and \`: the start whatever the flags */
	AS_EOT,      /* \z and \': the end whatever the flags */
	AS_WORDB,    /* \b */
	AS_NWORDB,   /* \B */
	AS_WBEG,     /* \< */
	AS_WEND      /* \> */
};

#define IS_CONSUMING(op) ((op) <= OP_CLASS)

struct rx_inst {
	rx_u8 op;
	rx_u8 arg;
	rx_u16 pad;
	rx_i32 x;
	rx_i32 y;
};

/* Named classes, for code points the bitmap doesn't cover (RX_UTF8, 256
 * and up) and to decide [:upper:] etc. */
#define P_ALPHA  0x001
#define P_DIGIT  0x002
#define P_ALNUM  0x004
#define P_UPPER  0x008
#define P_LOWER  0x010
#define P_SPACE  0x020
#define P_BLANK  0x040
#define P_PUNCT  0x080
#define P_PRINT  0x100
#define P_GRAPH  0x200
#define P_CNTRL  0x400
#define P_XDIGIT 0x800
#define P_WORD   0x1000

struct rx_class {
	rx_u8 bits[32];   /* characters 0-255; negation already in them */
	rx_u8 neg;        /* the class is negated (for code points 256 and up) */
	rx_u8 icase;
	rx_u8 nl_out;     /* negated under RX_NEWLINE: never newline */
	rx_u8 pad;
	rx_u32 props;     /* code points 256 and up that have one of these */
	rx_u32 nprops;    /* or lack one of these (\W \D \S in a bracket) */
	rx_u32 first;     /* ranges[first .. first + count) */
	rx_u32 count;
};

struct rx_prog {
	rx_u32 ninst;
	rx_u32 nclass;
	rx_u32 nrange;
	rx_u32 nsub;
	rx_u32 nmarks;
	rx_u32 cflags;
	rx_u8 backtrack;  /* back-references or look-aheads: the backtracker */
	rx_u8 has_back;   /* back-references (no memo) */
	rx_u8 has_look;
	rx_u8 anchored;   /* every match starts at the start */
	rx_u8 longest;    /* POSIX leftmost-longest (else leftmost-first) */
	rx_u8 has_first;  /* first[] says which bytes a match can start with */
	rx_u8 pad[2];
	rx_u8 first[32];
	struct rx_inst *inst;
	struct rx_class *cls;
	rx_u32 *ranges;   /* pairs: lo, hi */
};

/* rx_unicode.c */
rx_u32 rx_lower(rx_u32 c, int utf8);
rx_u32 rx_upper(rx_u32 c, int utf8);
rx_u32 rx_fold(rx_u32 c, int utf8);
int rx_hasprop(rx_u32 c, rx_u32 props, int utf8);
int rx_isword(rx_u32 c, int utf8);
/* decodes one character at s[*i], s[0..n); advances *i. Invalid bytes come
 * back as 0x110000 + byte. */
rx_u32 rx_utf8_next(const rx_u8 *s, rx_u32 n, rx_u32 *i);
rx_u32 rx_utf8_prev(const rx_u8 *s, rx_u32 start, rx_u32 i);
int rx_utf8_put(rx_u32 c, rx_u8 *out);

/* rx_exec.c helpers the compiler uses */
int rx_class_has(const struct rx_prog *p, const struct rx_class *k, rx_u32 c);

#define BIT_SET(b, c) ((b)[(c) >> 3] |= (rx_u8)(1u << ((c) & 7)))
#define BIT_HAS(b, c) ((b)[(c) >> 3] & (1u << ((c) & 7)))

#define RX_MAX_INST 32767
#define RX_DUP_MAX 255
#define RX_INVALID_BASE 0x110000u

void rx_memset(void *d, int c, rx_u32 n);
void rx_memcpy(void *d, const void *s, rx_u32 n);

#endif
