/* GFOpenRegex matchers. Programs without back-references or look-aheads
 * run on a Pike VM (Thompson's construction with captures): every
 * position once, time in proportion to subject times pattern, no
 * recursion and no stack use that grows with the input. The rest run on a
 * backtracker whose choice points live on the heap, with a step limit.
 * Both give POSIX leftmost-longest matches, or Perl's leftmost-first with
 * RX_PERL or RX_FIRSTMATCH. MIT, Copyright (c) 2026 AmiCradle Team. */
#include "rx_internal.h"

#define NONE 0xFFFFFFFFu
#define RX_STEP_LIMIT 20000000u

struct ctx {
	const rx_u8 *s;
	rx_u32 start, end, eflags;
	int utf8;
	const struct rx_prog *p;
};

static rx_u32 char_at(const struct ctx *x, rx_u32 pos, rx_u32 *next)
{
	if (pos >= x->end) { *next = pos; return NONE; }
	if (!x->utf8) { *next = pos + 1; return x->s[pos]; }
	*next = pos;
	return rx_utf8_next(x->s, x->end, next);
}

static rx_u32 char_before(const struct ctx *x, rx_u32 pos)
{
	if (pos <= x->start) return NONE;
	if (!x->utf8) return x->s[pos - 1];
	return rx_utf8_prev(x->s, x->start, pos);
}

int rx_class_has(const struct rx_prog *p, const struct rx_class *k, rx_u32 c)
{
	int raw = 0, utf8;
	rx_u32 i;
	if (c < 256) return BIT_HAS(k->bits, c) != 0;
	if (c >= RX_INVALID_BASE) return k->neg;
	utf8 = (p->cflags & RX_UTF8) != 0;
	for (i = 0; i < k->count && !raw; i++) {
		const rx_u32 *r = &p->ranges[2 * (k->first + i)];
		if (c >= r[0] && c <= r[1]) raw = 1;
		else if (k->icase) {
			rx_u32 l = rx_lower(c, 1), u = rx_upper(c, 1);
			if ((l >= r[0] && l <= r[1]) || (u >= r[0] && u <= r[1])) raw = 1;
		}
	}
	if (!raw && k->icase) {
		rx_u32 l = rx_lower(c, 1), u = rx_upper(c, 1);
		if ((l < 256 && BIT_HAS(k->bits, l) && !k->neg) || (u < 256 && BIT_HAS(k->bits, u) && !k->neg))
			raw = 1;
	}
	if (!raw && k->props && rx_hasprop(c, k->props, utf8)) raw = 1;
	if (!raw && k->nprops) {
		rx_u32 b;
		for (b = 1; b && b <= k->nprops; b <<= 1)
			if ((k->nprops & b) && !rx_hasprop(c, b, utf8)) { raw = 1; break; }
	}
	return k->neg ? !raw : raw;
}

static int consume_ok(const struct ctx *x, const struct rx_inst *in, rx_u32 c)
{
	switch (in->op) {
	case OP_BYTE: return c == (rx_u32)in->x;
	case OP_BYTE2: return c == (rx_u32)in->x || c == (rx_u32)in->y;
	case OP_CHAR: return c == (rx_u32)in->x;
	case OP_CHARI: return c < RX_INVALID_BASE && rx_fold(c, 1) == (rx_u32)in->x;
	case OP_ANY: return !(in->arg && c == '\n');
	case OP_CLASS: return rx_class_has(x->p, &x->p->cls[in->x], c);
	}
	return 0;
}

static int assert_ok(const struct ctx *x, int kind, rx_u32 pos)
{
	rx_u32 nb = x->eflags & RX_NOTBOL, ne = x->eflags & RX_NOTEOL;
	switch (kind) {
	case AS_BOL: return pos == x->start && !nb;
	case AS_BOLM: return (pos == x->start && !nb) || (pos > x->start && x->s[pos - 1] == '\n');
	case AS_EOL: return pos == x->end && !ne;
	case AS_EOLM: return (pos == x->end && !ne) || (pos < x->end && x->s[pos] == '\n');
	case AS_EOLP: return !ne && (pos == x->end || (pos + 1 == x->end && x->s[pos] == '\n'));
	case AS_BOT: return pos == x->start;
	case AS_EOT: return pos == x->end;
	default: {
		rx_u32 n, a = char_before(x, pos), b = char_at(x, pos, &n);
		int wa = a != NONE && rx_isword(a, x->utf8);
		int wb = b != NONE && rx_isword(b, x->utf8);
		switch (kind) {
		case AS_WORDB: return wa != wb;
		case AS_NWORDB: return wa == wb;
		case AS_WBEG: return !wa && wb;
		case AS_WEND: return wa && !wb;
		}
	}
	}
	return 0;
}

/* the next position a match could start at, from pos */
static rx_u32 next_start(const struct ctx *x, rx_u32 pos)
{
	if (!x->p->has_first) return pos;
	while (pos < x->end && !BIT_HAS(x->p->first, x->s[pos])) pos++;
	return pos;
}

static void put_matches(const rx_i32 *best, rx_u32 kept, rx_u32 nmatch, struct rx_match *m)
{
	rx_u32 i;
	for (i = 0; i < nmatch; i++) {
		if (2 * i + 1 < kept && best[2 * i] >= 0 && best[2 * i + 1] >= best[2 * i]) {
			m[i].so = best[2 * i];
			m[i].eo = best[2 * i + 1];
		} else {
			m[i].so = -1;
			m[i].eo = -1;
		}
	}
}

/* ---- Pike VM ------------------------------------------------------------ */

struct tlist {
	rx_u32 n, stamp;
	rx_u32 *pc;
	rx_i32 *slots;
};

struct pike {
	const struct ctx *x;
	const struct rx_inst *inst;
	rx_u32 kept;     /* capture slots */
	rx_u32 width;    /* kept, then the loop marks */
	rx_u32 *stamp;   /* per instruction: the stamp of the list it is on */
	rx_i32 *stack;   /* pairs: pc (or -1 - slot for a restore), value */
};

static void add_thread(struct pike *v, struct tlist *l, rx_u32 pc0, rx_i32 *slots, rx_u32 pos)
{
	rx_i32 *st = v->stack;
	rx_u32 sp = 0, kept = v->kept, width = v->width;
	st[sp++] = (rx_i32)pc0;
	st[sp++] = 0;
	while (sp) {
		rx_i32 val = st[--sp], e = st[--sp];
		const struct rx_inst *in;
		rx_u32 pc;
		if (e < 0) { slots[-1 - e] = val; continue; }
		pc = (rx_u32)e;
		in = &v->inst[pc];
		/* a loop's check may be reached again with another mark */
		if (in->op != OP_CHK) {
			if (v->stamp[pc] == l->stamp) continue;
			v->stamp[pc] = l->stamp;
		}
		switch (in->op) {
		case OP_JMP:
			st[sp++] = in->x; st[sp++] = 0;
			break;
		case OP_SPLIT:
			st[sp++] = in->y; st[sp++] = 0;
			st[sp++] = in->x; st[sp++] = 0;
			break;
		case OP_SAVE:
			if ((rx_u32)in->x < kept) {
				st[sp++] = -1 - in->x; st[sp++] = slots[in->x];
				slots[in->x] = (rx_i32)pos;
			}
			st[sp++] = (rx_i32)pc + 1; st[sp++] = 0;
			break;
		case OP_MARK: {
			rx_u32 sl = kept + (rx_u32)in->x;
			st[sp++] = -1 - (rx_i32)sl; st[sp++] = slots[sl];
			slots[sl] = (rx_i32)pos;
			st[sp++] = (rx_i32)pc + 1; st[sp++] = 0;
			break;
		}
		case OP_CHK:
			st[sp++] = slots[kept + (rx_u32)in->x] == (rx_i32)pos ? in->y : (rx_i32)pc + 1;
			st[sp++] = 0;
			break;
		case OP_ASSERT:
			if (assert_ok(v->x, in->arg, pos)) { st[sp++] = (rx_i32)pc + 1; st[sp++] = 0; }
			break;
		default: {
			rx_u32 n = l->n++, i;
			rx_i32 *d = &l->slots[n * width];
			l->pc[n] = pc;
			for (i = 0; i < width; i++) d[i] = slots[i];
			break;
		}
		}
	}
}

static int pike_run(struct rx_env *env, const struct ctx *x, rx_u32 nmatch, struct rx_match *m)
{
	const struct rx_prog *p = x->p;
	rx_u32 ni = p->ninst, kept, w, size, i, pos, stampc = 0;
	rx_u8 *mem;
	struct pike v;
	struct tlist la, lb, *cl = &la, *nl = &lb, *t;
	rx_i32 *scratch, *best;
	int matched = 0;
	kept = 2 * (nmatch < p->nsub + 1 ? nmatch : p->nsub + 1);
	if (kept < 2) kept = 2;
	w = kept + p->nmarks;
	size = ni * 4 * 3                    /* two pc arrays, stamps */
	     + ni * w * 4 * 2                /* two slot arrays */
	     + (3 * ni + 4) * 2 * 4          /* the stack */
	     + w * 4 * 2;                    /* scratch, best */
	mem = (rx_u8 *)env->alloc(env, size);
	if (!mem) return RX_ESPACE;
	la.pc = (rx_u32 *)mem;
	lb.pc = la.pc + ni;
	v.stamp = lb.pc + ni;
	la.slots = (rx_i32 *)(v.stamp + ni);
	lb.slots = la.slots + ni * w;
	v.stack = lb.slots + ni * w;
	scratch = v.stack + (3 * ni + 4) * 2;
	best = scratch + w;
	for (i = 0; i < ni; i++) v.stamp[i] = 0;
	v.x = x;
	v.inst = p->inst;
	v.kept = kept;
	v.width = w;
	la.n = lb.n = 0;
	la.stamp = ++stampc;
	pos = x->start;
	for (;;) {
		rx_u32 c, next;
		if (!matched && (!p->anchored || pos == x->start)) {
			if (cl->n == 0 && !p->anchored) {
				pos = next_start(x, pos);
				if (p->has_first && pos >= x->end) break;
			}
			for (i = 0; i < w; i++) scratch[i] = -1;
			add_thread(&v, cl, 0, scratch, pos);
		}
		if (cl->n == 0) {
			if (matched || p->anchored || pos >= x->end) break;
			char_at(x, pos, &next);
			pos = next;
			cl->stamp = ++stampc;
			continue;
		}
		c = char_at(x, pos, &next);
		nl->n = 0;
		nl->stamp = ++stampc;
		for (i = 0; i < cl->n; i++) {
			rx_u32 pc = cl->pc[i];
			rx_i32 *sl = &cl->slots[i * w];
			const struct rx_inst *in = &p->inst[pc];
			if (matched && p->longest && sl[0] > best[0]) continue;
			if (in->op == OP_MATCH) {
				if (!p->longest) {
					rx_u32 k;
					for (k = 0; k < kept; k++) best[k] = sl[k];
					matched = 1;
					break;
				}
				if (!matched || sl[0] < best[0] || (sl[0] == best[0] && sl[1] > best[1])) {
					rx_u32 k;
					for (k = 0; k < kept; k++) best[k] = sl[k];
					matched = 1;
				}
				continue;
			}
			if (c != NONE && consume_ok(x, in, c)) {
				rx_u32 k;
				for (k = 0; k < w; k++) scratch[k] = sl[k];
				add_thread(&v, nl, pc + 1, scratch, next);
			}
		}
		t = cl; cl = nl; nl = t;
		if (pos >= x->end) break;
		pos = next;
	}
	if (matched) put_matches(best, kept, nmatch, m);
	env->free(env, mem);
	return matched ? RX_OK : RX_NOMATCH;
}

/* ---- Backtracker -------------------------------------------------------- */

enum { BT_CHOICE, BT_RESTORE, BT_LOOK };

struct bt_ent {
	rx_i32 kind, a, b, c;
};

struct bt {
	struct rx_env *env;
	struct bt_ent *st;
	rx_u32 sp, cap;
};

static int bt_push(struct bt *b, rx_i32 kind, rx_i32 a, rx_i32 v, rx_i32 c)
{
	if (b->sp == b->cap) {
		rx_u32 n = b->cap ? b->cap * 2 : 256;
		struct bt_ent *q = (struct bt_ent *)b->env->alloc(b->env, n * sizeof(struct bt_ent));
		if (!q) return 0;
		if (b->st) {
			rx_memcpy(q, b->st, b->sp * sizeof(struct bt_ent));
			b->env->free(b->env, b->st);
		}
		b->st = q;
		b->cap = n;
	}
	b->st[b->sp].kind = kind;
	b->st[b->sp].a = a;
	b->st[b->sp].b = v;
	b->st[b->sp].c = c;
	b->sp++;
	return 1;
}

static int backref_ok(const struct ctx *x, const rx_i32 *slots, const struct rx_inst *in,
                      rx_u32 *pos)
{
	rx_i32 so = slots[2 * in->x], eo = slots[2 * in->x + 1];
	rx_u32 a, b;
	if (so < 0 || eo < so) return 0;
	a = (rx_u32)so;
	b = *pos;
	if (!in->arg) {
		rx_u32 n = (rx_u32)(eo - so), i;
		if (b + n > x->end) return 0;
		for (i = 0; i < n; i++)
			if (x->s[a + i] != x->s[b + i]) return 0;
		*pos = b + n;
		return 1;
	}
	while (a < (rx_u32)eo) {
		rx_u32 na, nb, ca, cb;
		ca = char_at(x, a, &na);
		if (na > (rx_u32)eo) return 0;
		cb = char_at(x, b, &nb);
		if (cb == NONE) return 0;
		if (ca != cb && rx_fold(ca, x->utf8) != rx_fold(cb, x->utf8)) return 0;
		a = na;
		b = nb;
	}
	*pos = b;
	return 1;
}

/* With no back-references and no look-aheads, a state (instruction,
 * position) reached again goes the same way as the first time, unless a
 * loop mark equals the position (an iteration that has taken nothing yet
 * ends differently): those states are not remembered. So each state is
 * tried once and the time is in proportion to subject times pattern. */
#define RX_MEMO_BITS (1u << 21)

static int bt_run(struct rx_env *env, const struct ctx *x, rx_u32 nmatch, struct rx_match *m,
                  int memo)
{
	const struct rx_prog *p = x->p;
	rx_u32 ncap = 2 * (p->nsub + 1), nslots = ncap + p->nmarks, i, s, steps = 0, span;
	rx_i32 *slots, *best;
	rx_u8 *seen = 0;
	struct bt b;
	int result = RX_NOMATCH, found = 0;
	span = x->end - x->start + 1;
	slots = (rx_i32 *)env->alloc(env, 2 * nslots * sizeof(rx_i32));
	if (!slots) return RX_ESPACE;
	best = slots + nslots;
	if (memo) {
		rx_u32 bytes = (p->ninst * span + 7) / 8;
		seen = (rx_u8 *)env->alloc(env, bytes);
		if (!seen) { env->free(env, slots); return RX_ESPACE; }
		rx_memset(seen, 0, bytes);
	}
	b.env = env;
	b.st = 0;
	b.sp = b.cap = 0;
	s = x->start;
	for (;;) {
		if (!p->anchored) s = next_start(x, s);
		else if (s != x->start) break;
		if (p->has_first && s >= x->end) break;
		for (i = 0; i < nslots; i++) slots[i] = -1;
		b.sp = 0;
		if (!bt_push(&b, BT_CHOICE, 0, (rx_i32)s, 0)) { result = RX_ESPACE; goto out; }
		while (b.sp) {
			struct bt_ent e = b.st[--b.sp];
			rx_u32 pc, pos;
			if (e.kind == BT_RESTORE) { slots[e.a] = e.b; continue; }
			if (e.kind == BT_LOOK) {
				/* the look-ahead's body failed every way */
				if (!e.c) continue;
				pc = (rx_u32)e.a;
				pos = (rx_u32)e.b;
			} else {
				pc = (rx_u32)e.a;
				pos = (rx_u32)e.b;
			}
			for (;;) {
				const struct rx_inst *in = &p->inst[pc];
				if (seen) {
					rx_u32 bit = pc * span + (pos - x->start), k;
					for (k = 0; k < p->nmarks; k++)
						if (slots[ncap + k] == (rx_i32)pos) break;
					if (k == p->nmarks) {
						if (BIT_HAS(seen, bit)) goto fail;
						BIT_SET(seen, bit);
					}
				} else if (++steps > RX_STEP_LIMIT) { result = RX_ESPACE; goto out; }
				if (IS_CONSUMING(in->op)) {
					rx_u32 next, c = char_at(x, pos, &next);
					if (c == NONE || !consume_ok(x, in, c)) break;
					pos = next;
					pc++;
					continue;
				}
				switch (in->op) {
				case OP_MATCH:
					if (!found || (p->longest && slots[1] > best[1])) {
						for (i = 0; i < nslots; i++) best[i] = slots[i];
						found = 1;
					}
					if (!p->longest) goto done;
					goto fail;
				case OP_SPLIT:
					if (!bt_push(&b, BT_CHOICE, in->y, (rx_i32)pos, 0)) { result = RX_ESPACE; goto out; }
					pc = (rx_u32)in->x;
					continue;
				case OP_JMP:
					pc = (rx_u32)in->x;
					continue;
				case OP_SAVE:
				case OP_MARK: {
					rx_u32 sl = in->op == OP_SAVE ? (rx_u32)in->x : ncap + (rx_u32)in->x;
					if (!bt_push(&b, BT_RESTORE, (rx_i32)sl, slots[sl], 0)) { result = RX_ESPACE; goto out; }
					slots[sl] = (rx_i32)pos;
					pc++;
					continue;
				}
				case OP_CHK:
					if (slots[ncap + (rx_u32)in->x] == (rx_i32)pos) pc = (rx_u32)in->y;
					else pc++;
					continue;
				case OP_ASSERT:
					if (!assert_ok(x, in->arg, pos)) goto fail;
					pc++;
					continue;
				case OP_BACKREF:
					if (!backref_ok(x, slots, in, &pos)) goto fail;
					pc++;
					continue;
				case OP_LOOK:
					if (!bt_push(&b, BT_LOOK, in->x, (rx_i32)pos, in->arg)) { result = RX_ESPACE; goto out; }
					pc++;
					continue;
				case OP_LOOKEND: {
					rx_u32 k = b.sp, w;
					struct bt_ent bar;
					while (k > 0 && b.st[k - 1].kind != BT_LOOK) k--;
					if (k == 0) { result = RX_ASSERT; goto out; }
					bar = b.st[k - 1];
					if (bar.c) {
						/* negative: the body matched, so the look-ahead fails */
						while (b.sp > k) {
							struct bt_ent r = b.st[--b.sp];
							if (r.kind == BT_RESTORE) slots[r.a] = r.b;
						}
						b.sp--;
						goto fail;
					}
					/* positive: drop the body's choices, keep its undo records */
					w = k - 1;
					for (i = k; i < b.sp; i++)
						if (b.st[i].kind == BT_RESTORE) b.st[w++] = b.st[i];
					b.sp = w;
					pc = (rx_u32)bar.a;
					pos = (rx_u32)bar.b;
					continue;
				}
				default:
					result = RX_ASSERT;
					goto out;
				}
			}
		fail:
			;
		}
		if (found) break;
		if (s >= x->end) break;
		{
			rx_u32 next;
			char_at(x, s, &next);
			s = next;
		}
	}
done:
	if (found) {
		put_matches(best, ncap, nmatch, m);
		result = RX_OK;
	}
out:
	if (seen) env->free(env, seen);
	if (b.st) env->free(env, b.st);
	env->free(env, slots);
	return result;
}

int rx_exec(struct rx_env *env, const struct rx_prog *prog, const char *subject,
            rx_u32 start, rx_u32 end, rx_u32 nmatch, struct rx_match *m, rx_u32 eflags)
{
	struct ctx x;
	if (!prog || (!subject && end)) return RX_INVARG;
	if (start > end) return RX_INVARG;
	if (prog->cflags & RX_NOSUB) nmatch = 0;
	if (nmatch && !m) nmatch = 0;
	x.s = (const rx_u8 *)subject;
	x.start = start;
	x.end = end;
	x.eflags = eflags;
	x.utf8 = (prog->cflags & RX_UTF8) != 0;
	x.p = prog;
	if (prog->backtrack) return bt_run(env, &x, nmatch, m, 0);
	/* Perl's empty-iteration rule needs the loop marks, which a Pike VM
	 * can't keep apart: leftmost-first programs with nullable loops take
	 * the remembering backtracker while its table is small enough */
	if (!prog->longest && prog->nmarks &&
	    (end - start + 1) <= RX_MEMO_BITS / (prog->ninst ? prog->ninst : 1))
		return bt_run(env, &x, nmatch, m, 1);
	return pike_run(env, &x, nmatch, m);
}
