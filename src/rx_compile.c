/* GFOpenRegex compiler: a pattern (POSIX basic or extended, or Perl syntax)
 * straight to a program for the matchers, with no syntax tree and no
 * recursion: groups live on a heap stack of frames, and every atom is a
 * run of instructions that a quantifier copies or wraps.
 * Written from POSIX.1-2017 XBD 9 and perlre. MIT, Copyright (c) 2026
 * AmiCradle Team. */
#include "rx_internal.h"

#define SYN_BRE 0
#define SYN_ERE 1
#define SYN_PERL 2

#define F_XMODE 0x20000 /* Perl (?x) */

enum { K_TOP, K_CAP, K_NONCAP, K_LOOK, K_NLOOK };

struct frame {
	rx_i32 group_start; /* first instruction of the group (its SAVE or LOOK) */
	rx_i32 alt_start;   /* first instruction of the current alternative */
	rx_i32 atom_start;  /* the last atom, for a quantifier; -1 none */
	rx_i32 jmps;        /* chain of JMPs to the group's end, through x */
	rx_i32 cap;
	rx_u32 kind;
	rx_u32 flags;       /* RX_ICASE, RX_DOTALL, RX_MULTILINE, F_XMODE here */
	rx_u32 last_quant;  /* the last token was a quantifier */
};

struct comp {
	struct rx_env *env;
	const rx_u8 *pat;
	rx_u32 len, pos;
	rx_u32 cflags;
	int syn, utf8, err;
	struct rx_inst *inst;
	rx_u32 ninst, cap_inst;
	struct rx_class *cls;
	rx_u32 nclass, cap_cls;
	rx_u32 *ranges;
	rx_u32 nrange, cap_range; /* in rx_u32s: two per range */
	struct frame *fr;
	rx_u32 nfr, cap_fr;
	rx_u32 ncap, nmarks;
	rx_u8 has_back, has_look;
};

static int grow(struct comp *c, void **p, rx_u32 *cap, rx_u32 need, rx_u32 size)
{
	rx_u32 n;
	void *q;
	if (need <= *cap) return 1;
	n = *cap ? *cap * 2 : 16;
	while (n < need) n *= 2;
	q = c->env->alloc(c->env, n * size);
	if (!q) { c->err = RX_ESPACE; return 0; }
	if (*p) {
		rx_memcpy(q, *p, *cap * size);
		c->env->free(c->env, *p);
	}
	*p = q;
	*cap = n;
	return 1;
}

static rx_i32 emit(struct comp *c, int op, int arg, rx_i32 x, rx_i32 y)
{
	struct rx_inst *i;
	if (c->err) return -1;
	if (c->ninst >= RX_MAX_INST) { c->err = RX_ESPACE; return -1; }
	if (!grow(c, (void **)&c->inst, &c->cap_inst, c->ninst + 1, sizeof(struct rx_inst)))
		return -1;
	i = &c->inst[c->ninst];
	i->op = (rx_u8)op;
	i->arg = (rx_u8)arg;
	i->pad = 0;
	i->x = x;
	i->y = y;
	return (rx_i32)c->ninst++;
}

static int has_x(int op) { return op == OP_SPLIT || op == OP_JMP || op == OP_LOOK; }
static int has_y(int op) { return op == OP_SPLIT || op == OP_CHK; }

/* Opens k slots at p: instructions from p move up, and their jumps to
 * anywhere in [p, ninst] move with them. Jumps from before p to p now
 * reach what is put there. */
static int insert_at(struct comp *c, rx_u32 p, rx_u32 k)
{
	rx_u32 n = c->ninst, i;
	if (c->err) return 0;
	if (n + k > RX_MAX_INST) { c->err = RX_ESPACE; return 0; }
	if (!grow(c, (void **)&c->inst, &c->cap_inst, n + k, sizeof(struct rx_inst))) return 0;
	for (i = n; i-- > p;)
		c->inst[i + k] = c->inst[i];
	for (i = p + k; i < n + k; i++) {
		struct rx_inst *in = &c->inst[i];
		if (has_x(in->op) && in->x >= (rx_i32)p && in->x <= (rx_i32)n) in->x += (rx_i32)k;
		if (has_y(in->op) && in->y >= (rx_i32)p && in->y <= (rx_i32)n) in->y += (rx_i32)k;
	}
	c->ninst = n + k;
	return 1;
}

/* ---- Quantifiers -------------------------------------------------------- */

/* can tmp[0..L) (jumps relative to a) be passed without taking a character? */
static int nullable(struct comp *c, const struct rx_inst *t, rx_u32 L, rx_i32 a)
{
	rx_u8 *seen;
	rx_u32 *work, nw = 0, pc;
	int result = 0;
	seen = (rx_u8 *)c->env->alloc(c->env, L + 1);
	work = (rx_u32 *)c->env->alloc(c->env, (2 * L + 2) * sizeof(rx_u32));
	if (!seen || !work) {
		c->err = RX_ESPACE;
		if (seen) c->env->free(c->env, seen);
		if (work) c->env->free(c->env, work);
		return 0;
	}
	rx_memset(seen, 0, L + 1);
	work[nw++] = 0;
	while (nw && !result) {
		pc = work[--nw];
		if (pc > L || seen[pc]) continue;
		seen[pc] = 1;
		if (pc == L) { result = 1; break; }
		switch (t[pc].op) {
		case OP_SPLIT:
			work[nw++] = (rx_u32)(t[pc].y - a);
			work[nw++] = (rx_u32)(t[pc].x - a);
			break;
		case OP_JMP:
		case OP_LOOK:
			work[nw++] = (rx_u32)(t[pc].x - a);
			break;
		case OP_CHK:
			work[nw++] = (rx_u32)(t[pc].y - a);
			work[nw++] = pc + 1;
			break;
		case OP_SAVE: case OP_MARK: case OP_ASSERT: case OP_BACKREF:
		case OP_LOOKEND:
			work[nw++] = pc + 1;
			break;
		default:
			break;
		}
	}
	c->env->free(c->env, seen);
	c->env->free(c->env, work);
	return result;
}

static void emit_copy(struct comp *c, const struct rx_inst *t, rx_u32 L, rx_i32 a)
{
	rx_u32 i, base = c->ninst;
	if (c->err) return;
	if (!grow(c, (void **)&c->inst, &c->cap_inst, base + L, sizeof(struct rx_inst))) return;
	for (i = 0; i < L; i++) {
		struct rx_inst in = t[i];
		if (has_x(in.op) && in.x >= a && in.x <= a + (rx_i32)L) in.x = in.x - a + (rx_i32)base;
		if (has_y(in.op) && in.y >= a && in.y <= a + (rx_i32)L) in.y = in.y - a + (rx_i32)base;
		c->inst[base + i] = in;
	}
	c->ninst = base + L;
}

static void quantify(struct comp *c, struct frame *f, rx_u32 min, rx_i32 max, int lazy)
{
	rx_i32 a = f->atom_start;
	rx_u32 L = c->ninst - (rx_u32)a, i, copies;
	struct rx_inst *t;
	int null;
	copies = min + (max < 0 ? 1 : (rx_u32)max - min);
	if ((rx_u32)a + L * copies + 4 * copies + 4 > RX_MAX_INST) { c->err = RX_ESPACE; return; }
	if (L == 0) return;
	t = (struct rx_inst *)c->env->alloc(c->env, L * sizeof(struct rx_inst));
	if (!t) { c->err = RX_ESPACE; return; }
	rx_memcpy(t, &c->inst[a], L * sizeof(struct rx_inst));
	null = nullable(c, t, L, a);
	c->ninst = (rx_u32)a;
	for (i = 0; i < min; i++)
		emit_copy(c, t, L, a);
	if (max < 0) {
		if (min >= 1 && !null) {
			rx_i32 s = (rx_i32)(c->ninst - L);
			rx_i32 n = (rx_i32)c->ninst + 1;
			if (lazy) emit(c, OP_SPLIT, 0, n, s);
			else emit(c, OP_SPLIT, 0, s, n);
		} else {
			rx_i32 l0 = emit(c, OP_SPLIT, 0, 0, 0), mark = -1, chk = -1;
			if (null) {
				mark = (rx_i32)c->nmarks++;
				emit(c, OP_MARK, 0, mark, 0);
			}
			emit_copy(c, t, L, a);
			/* an iteration that took nothing ends the loop (as Perl) */
			if (null) chk = emit(c, OP_CHK, 0, mark, 0);
			emit(c, OP_JMP, 0, l0, 0);
			if (!c->err) {
				if (chk >= 0) c->inst[chk].y = (rx_i32)c->ninst;
				if (lazy) { c->inst[l0].x = (rx_i32)c->ninst; c->inst[l0].y = l0 + 1; }
				else { c->inst[l0].x = l0 + 1; c->inst[l0].y = (rx_i32)c->ninst; }
			}
		}
	} else {
		rx_i32 chain = -1;
		for (i = min; i < (rx_u32)max; i++) {
			rx_i32 s = emit(c, OP_SPLIT, 0, 0, chain);
			chain = s;
			emit_copy(c, t, L, a);
		}
		while (!c->err && chain >= 0) {
			rx_i32 next = c->inst[chain].y;
			if (lazy) { c->inst[chain].x = (rx_i32)c->ninst; c->inst[chain].y = chain + 1; }
			else { c->inst[chain].x = chain + 1; c->inst[chain].y = (rx_i32)c->ninst; }
			chain = next;
		}
	}
	c->env->free(c->env, t);
}

/* ---- Characters --------------------------------------------------------- */

static int at_end(struct comp *c) { return c->pos >= c->len; }
static rx_u32 peek(struct comp *c) { return c->pos < c->len ? c->pat[c->pos] : 0x7FFFFFFF; }
static rx_u32 peek2(struct comp *c) { return c->pos + 1 < c->len ? c->pat[c->pos + 1] : 0x7FFFFFFF; }

/* the next literal character: a byte, or a code point with RX_UTF8 */
static rx_u32 next_char(struct comp *c)
{
	if (c->utf8) return rx_utf8_next(c->pat, c->len, &c->pos);
	return c->pat[c->pos++];
}

static void emit_literal(struct comp *c, struct frame *f, rx_u32 ch)
{
	int icase = (f->flags & RX_ICASE) != 0;
	f->atom_start = (rx_i32)c->ninst;
	if (c->utf8 && ch >= RX_INVALID_BASE) { /* an invalid byte matches itself */
		emit(c, OP_CHAR, 0, (rx_i32)ch, 0);
		return;
	}
	if (!c->utf8 || ch < 128) {
		rx_u32 b = ch;
		rx_u32 o = icase ? (rx_lower(b, 0) != b ? rx_lower(b, 0) : rx_upper(b, 0)) : b;
		if (o != b) emit(c, OP_BYTE2, 0, (rx_i32)b, (rx_i32)o);
		else emit(c, OP_BYTE, 0, (rx_i32)b, 0);
		return;
	}
	if (icase && (rx_lower(ch, 1) != ch || rx_upper(ch, 1) != ch || ch == 0x3C2))
		emit(c, OP_CHARI, 0, (rx_i32)rx_fold(ch, 1), 0);
	else
		emit(c, OP_CHAR, 0, (rx_i32)ch, 0);
}

/* ---- Bracket expressions and \d \w \s ------------------------------------ */

static struct rx_class *new_class(struct comp *c)
{
	struct rx_class *k;
	if (!grow(c, (void **)&c->cls, &c->cap_cls, c->nclass + 1, sizeof(struct rx_class)))
		return 0;
	k = &c->cls[c->nclass];
	rx_memset(k, 0, sizeof *k);
	k->first = c->nrange / 2;
	return k;
}

static void class_range(struct comp *c, struct rx_class *k, rx_u32 lo, rx_u32 hi)
{
	rx_u32 ch;
	for (ch = lo; ch <= hi && ch < 256; ch++)
		BIT_SET(k->bits, ch);
	if (lo >= RX_INVALID_BASE) { /* an invalid byte, as itself */
		BIT_SET(k->bits, lo - RX_INVALID_BASE);
		return;
	}
	if (hi >= 256 && c->utf8) {
		if (lo < 256) lo = 256;
		/* k is the last class: its ranges are the last ones */
		if (!grow(c, (void **)&c->ranges, &c->cap_range, c->nrange + 2, sizeof(rx_u32))) return;
		c->ranges[c->nrange++] = lo;
		c->ranges[c->nrange++] = hi;
		k->count++;
	}
}

static void class_props(struct comp *c, struct rx_class *k, rx_u32 prop, int neg)
{
	rx_u32 ch;
	for (ch = 0; ch < 256; ch++)
		if ((rx_hasprop(ch, prop, c->utf8) != 0) != neg)
			BIT_SET(k->bits, ch);
	if (neg) k->nprops |= prop;
	else k->props |= prop;
}

static const struct { const char *name; rx_u32 prop; } class_names[] = {
	{ "alpha", P_ALPHA }, { "digit", P_DIGIT }, { "alnum", P_ALNUM },
	{ "upper", P_UPPER }, { "lower", P_LOWER }, { "space", P_SPACE },
	{ "blank", P_BLANK }, { "punct", P_PUNCT }, { "print", P_PRINT },
	{ "graph", P_GRAPH }, { "cntrl", P_CNTRL }, { "xdigit", P_XDIGIT },
	{ "word", P_WORD }
};

static rx_u32 escape_prop(rx_u32 e, int *neg)
{
	*neg = e >= 'A' && e <= 'Z';
	switch (e | 0x20) {
	case 'd': return P_DIGIT;
	case 'w': return P_WORD;
	case 's': return P_SPACE;
	}
	return 0;
}

static int hexval(rx_u32 ch)
{
	if (ch >= '0' && ch <= '9') return (int)(ch - '0');
	if (ch >= 'a' && ch <= 'f') return (int)(ch - 'a' + 10);
	if (ch >= 'A' && ch <= 'F') return (int)(ch - 'A' + 10);
	return -1;
}

/* Perl's character escapes, after the backslash; -1 when e is not one */
static rx_i32 perl_char_escape(struct comp *c, rx_u32 e)
{
	switch (e) {
	case 'n': return '\n';
	case 't': return '\t';
	case 'r': return '\r';
	case 'f': return '\f';
	case 'v': return '\v';
	case 'e': return 27;
	case 'a': return 7;
	case '0': {
		rx_u32 v = 0, k;
		for (k = 0; k < 2 && peek(c) >= '0' && peek(c) <= '7'; k++)
			v = v * 8 + (c->pat[c->pos++] - '0');
		return (rx_i32)v;
	}
	case 'x': {
		rx_u32 v = 0;
		if (peek(c) == '{') {
			c->pos++;
			while (!at_end(c) && peek(c) != '}') {
				int h = hexval(c->pat[c->pos++]);
				if (h < 0 || v > 0x10FFFF) { c->err = RX_EESCAPE; return 0; }
				v = v * 16 + (rx_u32)h;
			}
			if (at_end(c)) { c->err = RX_EBRACE; return 0; }
			c->pos++;
		} else {
			int k;
			for (k = 0; k < 2 && hexval(peek(c)) >= 0; k++)
				v = v * 16 + (rx_u32)hexval(c->pat[c->pos++]);
		}
		if (!c->utf8 && v > 255) { c->err = RX_EESCAPE; return 0; }
		if (v > 0x10FFFF) { c->err = RX_EESCAPE; return 0; }
		return (rx_i32)v;
	}
	case 'c':
		if (at_end(c)) { c->err = RX_EESCAPE; return 0; }
		return (rx_i32)((c->pat[c->pos++] & 0x1F) ^ 0x00);
	}
	return -1;
}

static int ascii_alnum(rx_u32 ch)
{
	return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
}

/* one end of a range in a bracket: a character, or -1 with *prop set for
 * [:name:] or \d and friends (no range may use those) */
static rx_i32 bracket_item(struct comp *c, struct rx_class *k, int *was_class)
{
	rx_u32 ch;
	*was_class = 0;
	if (peek(c) == '[' && (peek2(c) == ':' || peek2(c) == '=' || peek2(c) == '.')) {
		rx_u32 kind = peek2(c), s, e, n;
		c->pos += 2;
		s = c->pos;
		while (c->pos + 1 < c->len && !(c->pat[c->pos] == kind && c->pat[c->pos + 1] == ']'))
			c->pos++;
		if (c->pos + 1 >= c->len) { c->err = RX_EBRACK; return -1; }
		e = c->pos;
		c->pos += 2;
		n = e - s;
		if (kind == ':') {
			unsigned i;
			for (i = 0; i < sizeof class_names / sizeof class_names[0]; i++) {
				const char *nm = class_names[i].name;
				rx_u32 j = 0;
				while (j < n && nm[j] && nm[j] == (char)c->pat[s + j]) j++;
				if (j == n && !nm[j]) {
					class_props(c, k, class_names[i].prop, 0);
					*was_class = 1;
					return -1;
				}
			}
			c->err = RX_ECTYPE;
			return -1;
		}
		/* [=c=] and [.c.]: one character (no multi-character elements) */
		{
			rx_u32 p = s;
			if (n == 0) { c->err = RX_ECOLLATE; return -1; }
			ch = c->utf8 ? rx_utf8_next(c->pat, e, &p) : c->pat[p++];
			if (p != e) { c->err = RX_ECOLLATE; return -1; }
			if (kind == '=') { class_range(c, k, ch, ch); *was_class = 1; return -1; }
			return (rx_i32)ch;
		}
	}
	if (c->syn == SYN_PERL && peek(c) == '\\') {
		rx_u32 e;
		rx_i32 v;
		int neg;
		rx_u32 prop;
		c->pos++;
		if (at_end(c)) { c->err = RX_EESCAPE; return -1; }
		e = c->pat[c->pos];
		prop = escape_prop(e, &neg);
		if (prop) {
			c->pos++;
			class_props(c, k, prop, neg);
			*was_class = 1;
			return -1;
		}
		if (e == 'b') { c->pos++; return 8; }
		if (ascii_alnum(e)) {
			c->pos++;
			v = perl_char_escape(c, e);
			if (c->err) return -1;
			if (v < 0) { c->err = RX_EESCAPE; return -1; }
			return v;
		}
		return (rx_i32)next_char(c);
	}
	return (rx_i32)next_char(c);
}

static rx_i32 parse_bracket(struct comp *c, struct frame *f)
{
	struct rx_class *k = new_class(c);
	int neg = 0, first = 1;
	rx_u32 idx = c->nclass;
	if (!k) return -1;
	c->nclass++;
	if (peek(c) == '^') { neg = 1; c->pos++; }
	for (;;) {
		rx_i32 lo, hi;
		int was;
		if (at_end(c)) { c->err = RX_EBRACK; return -1; }
		if (peek(c) == ']' && !first) { c->pos++; break; }
		first = 0;
		k = &c->cls[idx];
		lo = bracket_item(c, k, &was);
		if (c->err) return -1;
		k = &c->cls[idx];
		if (was) {
			if (peek(c) == '-' && peek2(c) != ']' && c->pos + 1 < c->len &&
			    c->syn != SYN_PERL) { c->err = RX_ERANGE; return -1; }
			continue;
		}
		if (peek(c) == '-' && peek2(c) != ']' && c->pos + 1 < c->len) {
			c->pos++;
			hi = bracket_item(c, k, &was);
			if (c->err) return -1;
			k = &c->cls[idx];
			if (was) {
				if (c->syn != SYN_PERL) { c->err = RX_ERANGE; return -1; }
				class_range(c, k, (rx_u32)lo, (rx_u32)lo);
				class_range(c, &c->cls[idx], '-', '-');
				continue;
			}
			if (hi < lo) { c->err = RX_ERANGE; return -1; }
			class_range(c, k, (rx_u32)lo, (rx_u32)hi);
		} else {
			class_range(c, k, (rx_u32)lo, (rx_u32)lo);
		}
		if (c->err) return -1;
	}
	k = &c->cls[idx];
	if (f->flags & RX_ICASE) {
		rx_u8 b[32];
		rx_u32 ch;
		rx_memcpy(b, k->bits, 32);
		for (ch = 0; ch < 256; ch++)
			if (BIT_HAS(b, ch)) {
				rx_u32 l = rx_lower(ch, c->utf8), u = rx_upper(ch, c->utf8);
				if (l < 256) BIT_SET(k->bits, l);
				if (u < 256) BIT_SET(k->bits, u);
			}
		k->icase = 1;
	}
	if (neg) {
		unsigned i;
		for (i = 0; i < 32; i++) k->bits[i] = (rx_u8)~k->bits[i];
		k->neg = 1;
		if (c->syn != SYN_PERL && (c->cflags & RX_NEWLINE)) {
			k->bits['\n' >> 3] &= (rx_u8)~(1u << ('\n' & 7));
			k->nl_out = 1;
		}
	}
	return (rx_i32)idx;
}

static void emit_prop_class(struct comp *c, struct frame *f, rx_u32 prop, int neg)
{
	struct rx_class *k = new_class(c);
	if (!k) return;
	c->nclass++;
	class_props(c, k, prop, 0);
	k = &c->cls[c->nclass - 1];
	if (neg) {
		unsigned i;
		for (i = 0; i < 32; i++) k->bits[i] = (rx_u8)~k->bits[i];
		k->neg = 1;
	}
	f->atom_start = (rx_i32)c->ninst;
	emit(c, OP_CLASS, 0, (rx_i32)(c->nclass - 1), 0);
}

/* ---- Groups ----------------------------------------------------------- */

static struct frame *push_frame(struct comp *c, rx_u32 kind, rx_u32 flags)
{
	struct frame *f;
	if (!grow(c, (void **)&c->fr, &c->cap_fr, c->nfr + 1, sizeof(struct frame))) return 0;
	f = &c->fr[c->nfr++];
	f->group_start = (rx_i32)c->ninst;
	f->jmps = -1;
	f->cap = -1;
	f->kind = kind;
	f->flags = flags;
	f->last_quant = 0;
	if (kind == K_CAP) {
		f->cap = (rx_i32)++c->ncap;
		emit(c, OP_SAVE, 0, 2 * f->cap, 0);
	} else if (kind == K_LOOK || kind == K_NLOOK) {
		emit(c, OP_LOOK, kind == K_NLOOK, 0, 0);
		c->has_look = 1;
	}
	f->alt_start = (rx_i32)c->ninst;
	f->atom_start = -1;
	return f;
}

static void patch_jmps(struct comp *c, struct frame *f)
{
	rx_i32 j = f->jmps;
	while (j >= 0) {
		rx_i32 next = c->inst[j].x;
		c->inst[j].x = (rx_i32)c->ninst;
		j = next;
	}
	f->jmps = -1;
}

static void close_group(struct comp *c)
{
	struct frame *f = &c->fr[c->nfr - 1];
	rx_i32 start = f->group_start;
	patch_jmps(c, f);
	if (f->kind == K_CAP) {
		emit(c, OP_SAVE, 0, 2 * f->cap + 1, 0);
	} else if (f->kind == K_LOOK || f->kind == K_NLOOK) {
		emit(c, OP_LOOKEND, 0, 0, 0);
		if (!c->err) c->inst[start].x = (rx_i32)c->ninst;
	}
	c->nfr--;
	f = &c->fr[c->nfr - 1];
	f->atom_start = start;
	f->last_quant = 0;
}

static void alternate(struct comp *c, struct frame *f)
{
	rx_i32 s = f->alt_start, j;
	if (!insert_at(c, (rx_u32)s, 1)) return;
	c->inst[s].op = OP_SPLIT;
	c->inst[s].arg = 0;
	c->inst[s].x = s + 1;
	j = emit(c, OP_JMP, 0, f->jmps, 0);
	if (j < 0) return;
	f->jmps = j;
	c->inst[s].y = (rx_i32)c->ninst;
	f->alt_start = (rx_i32)c->ninst;
	f->atom_start = -1;
	f->last_quant = 0;
}

/* Perl's (? forms; the "(?" is taken. Returns 0 when the group is opened
 * or the flags are set, and sets c->err on a bad one. */
static void perl_group(struct comp *c)
{
	struct frame *f = &c->fr[c->nfr - 1];
	rx_u32 ch = peek(c);
	if (ch == ':') { c->pos++; push_frame(c, K_NONCAP, f->flags); return; }
	if (ch == '=') { c->pos++; push_frame(c, K_LOOK, f->flags); return; }
	if (ch == '!') { c->pos++; push_frame(c, K_NLOOK, f->flags); return; }
	if (ch == '#') {
		while (!at_end(c) && peek(c) != ')') c->pos++;
		if (at_end(c)) { c->err = RX_EPAREN; return; }
		c->pos++;
		return;
	}
	if (ch == 'P' || ch == '<' || ch == '\'') {
		rx_u32 end;
		if (ch == 'P') c->pos++;
		if (peek(c) == '<' && (peek2(c) == '=' || peek2(c) == '!')) {
			c->err = RX_BADPAT; /* look-behind: not offered */
			return;
		}
		if (peek(c) != '<' && peek(c) != '\'') { c->err = RX_BADPAT; return; }
		end = peek(c) == '<' ? '>' : '\'';
		c->pos++;
		if (at_end(c) || peek(c) == end) { c->err = RX_BADPAT; return; }
		while (!at_end(c) && peek(c) != end) {
			if (!ascii_alnum(peek(c)) && peek(c) != '_') { c->err = RX_BADPAT; return; }
			c->pos++;
		}
		if (at_end(c)) { c->err = RX_EPAREN; return; }
		c->pos++;
		push_frame(c, K_CAP, f->flags);
		return;
	}
	{
		/* flags: (?imsx-imsx) or (?imsx-imsx:...) */
		rx_u32 on = 0, off = 0, *set = &on;
		for (;;) {
			if (at_end(c)) { c->err = RX_EPAREN; return; }
			ch = c->pat[c->pos++];
			if (ch == 'i') *set |= RX_ICASE;
			else if (ch == 's') *set |= RX_DOTALL;
			else if (ch == 'm') *set |= RX_MULTILINE;
			else if (ch == 'x') *set |= F_XMODE;
			else if (ch == '-' && set == &on) set = &off;
			else if (ch == ')') {
				f->flags = (f->flags | on) & ~off;
				return;
			} else if (ch == ':') {
				push_frame(c, K_NONCAP, (f->flags | on) & ~off);
				return;
			} else { c->err = RX_BADPAT; return; }
		}
	}
}

/* ---- Intervals -------------------------------------------------------- */

/* after the '{' (or "\{"): min[,[max]] then '}' (or "\}"). Returns 1 with
 * the bounds, 0 when it is not an interval (Perl and ERE take the '{'
 * literally then), and sets c->err when it is a bad one. */
static int interval(struct comp *c, rx_u32 *min, rx_i32 *max)
{
	rx_u32 p = c->pos, lo = 0, hi = 0, ndig = 0, hdig = 0;
	int comma = 0;
	while (p < c->len && c->pat[p] >= '0' && c->pat[p] <= '9') {
		if (lo <= 100000) lo = lo * 10 + (c->pat[p] - '0');
		p++; ndig++;
	}
	if (p < c->len && c->pat[p] == ',') {
		comma = 1;
		p++;
		while (p < c->len && c->pat[p] >= '0' && c->pat[p] <= '9') {
			if (hi <= 100000) hi = hi * 10 + (c->pat[p] - '0');
			p++; hdig++;
		}
	}
	if (c->syn == SYN_BRE) {
		if (p + 1 < c->len && c->pat[p] == '\\' && c->pat[p + 1] == '}') p += 2;
		else { c->err = p >= c->len || (p + 1 >= c->len) ? RX_EBRACE : RX_BADBR; return 0; }
	} else {
		if (p < c->len && c->pat[p] == '}') p++;
		else if (c->syn == SYN_PERL || ndig == 0) return 0;
		else { c->err = p >= c->len ? RX_EBRACE : RX_BADBR; return 0; }
	}
	if (ndig == 0) {
		if (c->syn == SYN_PERL) return 0;
		/* {,n}: as glibc, 0 to n */
		if (!comma) { c->err = RX_BADBR; return 0; }
	}
	if (lo > RX_DUP_MAX || (hdig && hi > RX_DUP_MAX)) { c->err = RX_BADBR; return 0; }
	*min = lo;
	*max = comma ? (hdig ? (rx_i32)hi : -1) : (rx_i32)lo;
	if (*max >= 0 && (rx_u32)*max < lo) { c->err = RX_BADBR; return 0; }
	c->pos = p;
	return 1;
}

/* ---- The parser --------------------------------------------------------- */

static void do_quant(struct comp *c, struct frame *f, rx_u32 min, rx_i32 max)
{
	int lazy = 0;
	if (c->syn == SYN_PERL) {
		if (f->last_quant) { c->err = RX_BADRPT; return; }
		if (peek(c) == '?') { lazy = 1; c->pos++; }
		else if (peek(c) == '+') { c->err = RX_BADRPT; return; } /* possessive */
	}
	quantify(c, f, min, max, lazy);
	f->last_quant = 1;
}

/* a quantifier character with no atom before it: literal in POSIX syntax
 * (as glibc), an error in Perl's */
static int no_atom(struct comp *c, struct frame *f)
{
	return f->atom_start < 0 || f->atom_start == (rx_i32)c->ninst;
}

static void emit_assert(struct comp *c, struct frame *f, int kind)
{
	emit(c, OP_ASSERT, kind, 0, 0);
	f->atom_start = -1;
	f->last_quant = 0;
}

static void backref(struct comp *c, struct frame *f, rx_u32 n)
{
	if (n == 0 || n > c->ncap) { c->err = RX_ESUBREG; return; }
	{
		/* a group can't refer to itself while it is open */
		rx_u32 i;
		for (i = 0; i < c->nfr; i++)
			if (c->fr[i].cap == (rx_i32)n) { c->err = RX_ESUBREG; return; }
	}
	f->atom_start = (rx_i32)c->ninst;
	emit(c, OP_BACKREF, (f->flags & RX_ICASE) != 0, (rx_i32)n, 0);
	c->has_back = 1;
	f->last_quant = 0;
}

/* one escape, after the backslash */
static void escape(struct comp *c, struct frame *f)
{
	rx_u32 e;
	int neg;
	rx_u32 prop;
	if (at_end(c)) { c->err = RX_EESCAPE; return; }
	e = c->pat[c->pos];
	if (e >= '1' && e <= '9') {
		c->pos++;
		backref(c, f, e - '0');
		return;
	}
	prop = escape_prop(e, &neg);
	if (prop && (prop != P_DIGIT || c->syn == SYN_PERL)) {
		c->pos++;
		emit_prop_class(c, f, prop, neg);
		f->last_quant = 0;
		return;
	}
	switch (e) {
	case 'b': c->pos++; emit_assert(c, f, AS_WORDB); return;
	case 'B': c->pos++; emit_assert(c, f, AS_NWORDB); return;
	}
	if (c->syn != SYN_PERL) {
		switch (e) {
		case '<': c->pos++; emit_assert(c, f, AS_WBEG); return;
		case '>': c->pos++; emit_assert(c, f, AS_WEND); return;
		case '`': c->pos++; emit_assert(c, f, AS_BOT); return;
		case '\'': c->pos++; emit_assert(c, f, AS_EOT); return;
		}
		/* any other escaped character is itself */
		emit_literal(c, f, next_char(c));
		f->last_quant = 0;
		return;
	}
	switch (e) {
	case 'A': c->pos++; emit_assert(c, f, AS_BOT); return;
	case 'z': c->pos++; emit_assert(c, f, AS_EOT); return;
	case 'Z': c->pos++; emit_assert(c, f, AS_EOLP); return;
	case 'g': {
		rx_u32 n = 0, braces;
		c->pos++;
		braces = peek(c) == '{';
		if (braces) c->pos++;
		if (!(peek(c) >= '0' && peek(c) <= '9')) { c->err = RX_ESUBREG; return; }
		while (peek(c) >= '0' && peek(c) <= '9' && n < 1000) n = n * 10 + (c->pat[c->pos++] - '0');
		if (braces) {
			if (peek(c) != '}') { c->err = RX_EBRACE; return; }
			c->pos++;
		}
		backref(c, f, n);
		return;
	}
	case 'Q': {
		/* \Q...\E: literal */
		c->pos++;
		while (!at_end(c)) {
			if (peek(c) == '\\' && peek2(c) == 'E') { c->pos += 2; break; }
			emit_literal(c, f, next_char(c));
		}
		f->last_quant = 0;
		return;
	}
	case 'E': c->pos++; return;
	}
	if (ascii_alnum(e)) {
		rx_i32 v;
		c->pos++;
		v = perl_char_escape(c, e);
		if (c->err) return;
		if (v < 0) { c->err = RX_EESCAPE; return; }
		emit_literal(c, f, (rx_u32)v);
		f->last_quant = 0;
		return;
	}
	emit_literal(c, f, next_char(c));
	f->last_quant = 0;
}

static void parse(struct comp *c)
{
	struct frame *f;
	int bre = c->syn == SYN_BRE, perl = c->syn == SYN_PERL;
	rx_u32 flags = c->cflags & (RX_ICASE | RX_DOTALL | RX_MULTILINE);
	emit(c, OP_SAVE, 0, 0, 0);
	if (!push_frame(c, K_TOP, flags)) return;
	while (!c->err && !at_end(c)) {
		rx_u32 ch;
		f = &c->fr[c->nfr - 1];
		if (f->flags & F_XMODE) {
			ch = peek(c);
			if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v') {
				c->pos++;
				continue;
			}
			if (ch == '#') {
				while (!at_end(c) && peek(c) != '\n') c->pos++;
				continue;
			}
		}
		if (c->cflags & RX_NOSPEC) {
			emit_literal(c, f, next_char(c));
			continue;
		}
		ch = c->pat[c->pos];
		if (bre) {
			if (ch == '\\') {
				rx_u32 d = peek2(c);
				if (d == '(') { c->pos += 2; push_frame(c, K_CAP, f->flags); continue; }
				if (d == ')') {
					c->pos += 2;
					if (c->nfr < 2) { c->err = RX_EPAREN; break; }
					close_group(c);
					continue;
				}
				if (d == '|') { c->pos += 2; alternate(c, f); continue; }
				if (d == '{') {
					rx_u32 min; rx_i32 max;
					if (no_atom(c, f)) { c->err = RX_BADRPT; break; }
					c->pos += 2;
					if (interval(c, &min, &max)) do_quant(c, f, min, max);
					else if (!c->err) c->err = RX_BADBR;
					continue;
				}
				if (d == '}') { c->err = RX_BADBR; break; }
				if (d == '+' || d == '?') {
					if (no_atom(c, f)) { c->pos += 2; emit_literal(c, f, d); continue; }
					c->pos += 2;
					do_quant(c, f, d == '+' ? 1 : 0, d == '+' ? -1 : 1);
					continue;
				}
				c->pos++;
				escape(c, f);
				continue;
			}
			if (ch == '*') {
				c->pos++;
				if (no_atom(c, f)) emit_literal(c, f, '*');
				else do_quant(c, f, 0, -1);
				continue;
			}
			if (ch == '^') {
				c->pos++;
				if (f->alt_start == (rx_i32)c->ninst)
					emit_assert(c, f, (c->cflags & RX_NEWLINE) ? AS_BOLM : AS_BOL);
				else
					emit_literal(c, f, '^');
				continue;
			}
			if (ch == '$') {
				int anchor;
				c->pos++;
				anchor = at_end(c) || (peek(c) == '\\' && (peek2(c) == ')' || peek2(c) == '|'));
				if (anchor) emit_assert(c, f, (c->cflags & RX_NEWLINE) ? AS_EOLM : AS_EOL);
				else emit_literal(c, f, '$');
				continue;
			}
		} else {
			switch (ch) {
			case '(':
				c->pos++;
				if (perl && peek(c) == '?') { c->pos++; perl_group(c); }
				else push_frame(c, K_CAP, f->flags);
				continue;
			case ')':
				c->pos++;
				if (c->nfr < 2) {
					/* ERE: a ) with no ( is itself (as glibc) */
					if (perl) { c->err = RX_EPAREN; break; }
					emit_literal(c, f, ')');
					f->last_quant = 0;
					continue;
				}
				close_group(c);
				continue;
			case '|':
				c->pos++;
				alternate(c, f);
				continue;
			case '*': case '+': case '?':
				c->pos++;
				if (no_atom(c, f)) {
					if (perl) { c->err = RX_BADRPT; break; }
					/* glibc: a leading * is literal; + and ? there are errors */
					if (ch == '*') emit_literal(c, f, '*');
					else c->err = RX_BADRPT;
					continue;
				}
				do_quant(c, f, ch == '+' ? 1 : 0, ch == '?' ? 1 : -1);
				continue;
			case '{': {
				rx_u32 min; rx_i32 max, save = (rx_i32)c->pos;
				c->pos++;
				if (interval(c, &min, &max)) {
					if (no_atom(c, f)) { c->err = RX_BADRPT; break; }
					do_quant(c, f, min, max);
					continue;
				}
				if (c->err) break;
				c->pos = (rx_u32)save + 1;
				emit_literal(c, f, '{');
				f->last_quant = 0;
				continue;
			}
			case '^':
				c->pos++;
				if (perl) emit_assert(c, f, (f->flags & RX_MULTILINE) ? AS_BOLM : AS_BOL);
				else emit_assert(c, f, (c->cflags & RX_NEWLINE) ? AS_BOLM : AS_BOL);
				continue;
			case '$':
				c->pos++;
				if (perl) emit_assert(c, f, (f->flags & RX_MULTILINE) ? AS_EOLM : AS_EOLP);
				else emit_assert(c, f, (c->cflags & RX_NEWLINE) ? AS_EOLM : AS_EOL);
				continue;
			case '\\':
				c->pos++;
				escape(c, f);
				continue;
			}
			if (c->err) break;
		}
		if (ch == '[') {
			rx_i32 k;
			c->pos++;
			k = parse_bracket(c, f);
			if (c->err) break;
			f->atom_start = (rx_i32)c->ninst;
			emit(c, OP_CLASS, 0, k, 0);
			f->last_quant = 0;
			continue;
		}
		if (ch == '.') {
			int nl;
			c->pos++;
			nl = perl ? !(f->flags & RX_DOTALL) : (c->cflags & RX_NEWLINE) != 0;
			f->atom_start = (rx_i32)c->ninst;
			emit(c, OP_ANY, nl, 0, 0);
			f->last_quant = 0;
			continue;
		}
		emit_literal(c, f, next_char(c));
		f->last_quant = 0;
	}
	if (c->err) return;
	if (c->nfr != 1) { c->err = RX_EPAREN; return; }
	patch_jmps(c, &c->fr[0]);
	emit(c, OP_SAVE, 0, 1, 0);
	emit(c, OP_MATCH, 0, 0, 0);
}

/* ---- After the parse ------------------------------------------------------ */

static void set_lead_bytes(rx_u8 *first, rx_u32 cp)
{
	rx_u8 b[4];
	rx_utf8_put(cp, b);
	BIT_SET(first, b[0]);
}

/* Which bytes can a match start with? 0 when any can (or none must). */
static int first_bytes(struct comp *c, struct rx_prog *p)
{
	rx_u8 *seen;
	rx_u32 *work, nw = 0, pc, ch;
	int ok = 1;
	seen = (rx_u8 *)c->env->alloc(c->env, p->ninst);
	work = (rx_u32 *)c->env->alloc(c->env, (2 * p->ninst + 2) * sizeof(rx_u32));
	if (!seen || !work) {
		if (seen) c->env->free(c->env, seen);
		if (work) c->env->free(c->env, work);
		return 0;
	}
	rx_memset(seen, 0, p->ninst);
	rx_memset(p->first, 0, 32);
	work[nw++] = 0;
	while (nw && ok) {
		const struct rx_inst *in;
		pc = work[--nw];
		if (seen[pc]) continue;
		seen[pc] = 1;
		in = &p->inst[pc];
		switch (in->op) {
		case OP_BYTE: BIT_SET(p->first, in->x); break;
		case OP_BYTE2: BIT_SET(p->first, in->x); BIT_SET(p->first, in->y); break;
		case OP_CHAR:
			set_lead_bytes(p->first, (rx_u32)in->x);
			break;
		case OP_CHARI:
			/* every code point that folds to x: x and its cases */
			set_lead_bytes(p->first, (rx_u32)in->x);
			set_lead_bytes(p->first, rx_upper((rx_u32)in->x, 1));
			if (in->x == 0x3C3) set_lead_bytes(p->first, 0x3C2);
			if (in->x == 'i') set_lead_bytes(p->first, 0x130);
			if (in->x == 0xFF) set_lead_bytes(p->first, 0x178);
			break;
		case OP_ANY:
			for (ch = 0; ch < 256; ch++)
				if (!(in->arg && ch == '\n')) BIT_SET(p->first, ch);
			break;
		case OP_CLASS: {
			const struct rx_class *k = &p->cls[in->x];
			unsigned i;
			for (i = 0; i < 32; i++) p->first[i] |= k->bits[i];
			if (c->utf8) /* any multi-byte character, and invalid bytes */
				for (ch = 0x80; ch < 256; ch++) BIT_SET(p->first, ch);
			break;
		}
		case OP_MATCH: case OP_BACKREF: case OP_LOOK:
			ok = 0;
			break;
		case OP_SPLIT:
			work[nw++] = (rx_u32)in->y;
			work[nw++] = (rx_u32)in->x;
			break;
		case OP_JMP:
			work[nw++] = (rx_u32)in->x;
			break;
		case OP_CHK:
			work[nw++] = (rx_u32)in->y;
			work[nw++] = pc + 1;
			break;
		default: /* SAVE, MARK, ASSERT: no character */
			work[nw++] = pc + 1;
			break;
		}
	}
	c->env->free(c->env, seen);
	c->env->free(c->env, work);
	return ok;
}

static int is_anchored(const struct rx_prog *p)
{
	rx_u32 pc = 0;
	while (pc < p->ninst && (p->inst[pc].op == OP_SAVE || p->inst[pc].op == OP_MARK))
		pc++;
	return pc < p->ninst && p->inst[pc].op == OP_ASSERT &&
	       (p->inst[pc].arg == AS_BOL || p->inst[pc].arg == AS_BOT);
}

static void cleanup(struct comp *c)
{
	if (c->inst) c->env->free(c->env, c->inst);
	if (c->cls) c->env->free(c->env, c->cls);
	if (c->ranges) c->env->free(c->env, c->ranges);
	if (c->fr) c->env->free(c->env, c->fr);
}

int rx_compile(struct rx_env *env, const char *pattern, rx_u32 len, rx_u32 cflags,
               struct rx_prog **out, rx_u32 *nsub)
{
	struct comp c;
	struct rx_prog *p;
	rx_u32 size, off_inst, off_cls, off_rng;
	rx_memset(&c, 0, sizeof c);
	c.env = env;
	c.pat = (const rx_u8 *)pattern;
	c.len = len;
	c.cflags = cflags;
	c.syn = (cflags & RX_PERL) ? SYN_PERL : (cflags & RX_EXTENDED) ? SYN_ERE : SYN_BRE;
	c.utf8 = (cflags & RX_UTF8) != 0;
	*out = 0;
	if (!pattern && len) return RX_INVARG;
	parse(&c);
	if (c.err) { cleanup(&c); return c.err; }
	off_inst = (sizeof(struct rx_prog) + 7) & ~7u;
	off_cls = off_inst + c.ninst * sizeof(struct rx_inst);
	off_rng = off_cls + c.nclass * sizeof(struct rx_class);
	size = off_rng + c.nrange * sizeof(rx_u32);
	p = (struct rx_prog *)env->alloc(env, size);
	if (!p) { cleanup(&c); return RX_ESPACE; }
	rx_memset(p, 0, sizeof *p);
	p->inst = (struct rx_inst *)((rx_u8 *)p + off_inst);
	p->cls = (struct rx_class *)((rx_u8 *)p + off_cls);
	p->ranges = (rx_u32 *)((rx_u8 *)p + off_rng);
	rx_memcpy(p->inst, c.inst, c.ninst * sizeof(struct rx_inst));
	if (c.nclass) rx_memcpy(p->cls, c.cls, c.nclass * sizeof(struct rx_class));
	if (c.nrange) rx_memcpy(p->ranges, c.ranges, c.nrange * sizeof(rx_u32));
	p->ninst = c.ninst;
	p->nclass = c.nclass;
	p->nrange = c.nrange / 2;
	p->nsub = c.ncap;
	p->nmarks = c.nmarks;
	p->cflags = cflags;
	p->has_back = c.has_back;
	p->has_look = c.has_look;
	p->backtrack = c.has_back || c.has_look || (cflags & RX_BACKTRACK) != 0;
	p->longest = !(cflags & (RX_PERL | RX_FIRSTMATCH));
	p->anchored = (rx_u8)is_anchored(p);
	p->has_first = (rx_u8)first_bytes(&c, p);
	cleanup(&c);
	*out = p;
	if (nsub) *nsub = p->nsub;
	return RX_OK;
}

void rx_free(struct rx_env *env, struct rx_prog *prog)
{
	if (prog) env->free(env, prog);
}

static const char *const messages[] = {
	"no error",
	"no match",
	"bad pattern",
	"bad collating element",
	"unknown character class name",
	"backslash at the end of the pattern",
	"back-reference to a group that does not exist",
	"[ without its ]",
	"( without its ) or ) without its (",
	"{ without its }",
	"bad count in { }",
	"range end before its start",
	"not enough memory",
	"repetition with nothing to repeat",
	"empty expression",
	"internal error",
	"bad argument",
	"regex.library could not be opened",
	"pattern is not valid UTF-8"
};

rx_u32 rx_error(int code, char *buf, rx_u32 size)
{
	const char *m;
	rx_u32 n, k;
	if (code < 0 || code >= (int)(sizeof messages / sizeof messages[0]))
		m = "unknown regex error";
	else
		m = messages[code];
	n = rx_strlen(m);
	if (buf && size) {
		k = n < size - 1 ? n : size - 1;
		rx_memcpy(buf, m, k);
		buf[k] = 0;
	}
	return n + 1;
}
