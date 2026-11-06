/* GFOpenRegex characters: UTF-8 (RFC 3629), case and the named classes.
 * Bytes are ISO 8859-1, the Amiga's own character set. With RX_UTF8 the
 * code points past 255 are classed by script blocks and cased by the
 * regular pairings of the Unicode code charts (Latin, Greek, Cyrillic,
 * Armenian, fullwidth Latin): no tables, no unicode.library.
 * MIT, Copyright (c) 2026 AmiCradle Team. */
#include "rx_internal.h"

void rx_memset(void *d, int c, rx_u32 n)
{
	rx_u8 *p = (rx_u8 *)d;
	while (n--) *p++ = (rx_u8)c;
}

void rx_memcpy(void *d, const void *s, rx_u32 n)
{
	rx_u8 *p = (rx_u8 *)d;
	const rx_u8 *q = (const rx_u8 *)s;
	while (n--) *p++ = *q++;
}

rx_u32 rx_strlen(const char *s)
{
	const char *p = s;
	while (*p) p++;
	return (rx_u32)(p - s);
}

rx_u32 rx_utf8_next(const rx_u8 *s, rx_u32 n, rx_u32 *i)
{
	rx_u32 p = *i, c = s[p], need, min, k;
	if (c < 0x80) { *i = p + 1; return c; }
	if (c >= 0xC2 && c <= 0xDF) { need = 1; c &= 0x1F; min = 0x80; }
	else if (c >= 0xE0 && c <= 0xEF) { need = 2; c &= 0x0F; min = 0x800; }
	else if (c >= 0xF0 && c <= 0xF4) { need = 3; c &= 0x07; min = 0x10000; }
	else goto bad;
	if (p + need >= n) goto bad;
	for (k = 1; k <= need; k++) {
		rx_u32 b = s[p + k];
		if ((b & 0xC0) != 0x80) goto bad;
		c = (c << 6) | (b & 0x3F);
	}
	if (c < min || c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF)) goto bad;
	*i = p + need + 1;
	return c;
bad:
	*i = p + 1;
	return RX_INVALID_BASE + s[p];
}

rx_u32 rx_utf8_prev(const rx_u8 *s, rx_u32 start, rx_u32 i)
{
	/* the character that ends at i: step back over up to three
	 * continuation bytes, and check that it decodes to end at i */
	rx_u32 b = i - 1, k;
	for (k = 0; k < 3 && b > start && (s[b] & 0xC0) == 0x80; k++)
		b--;
	for (; b < i; b++) {
		rx_u32 j = b, c = rx_utf8_next(s, i, &j);
		if (j == i) return c;
	}
	return RX_INVALID_BASE + s[i - 1];
}

int rx_utf8_put(rx_u32 c, rx_u8 *o)
{
	if (c >= RX_INVALID_BASE) { o[0] = (rx_u8)(c - RX_INVALID_BASE); return 1; }
	if (c < 0x80) { o[0] = (rx_u8)c; return 1; }
	if (c < 0x800) { o[0] = (rx_u8)(0xC0 | (c >> 6)); o[1] = (rx_u8)(0x80 | (c & 0x3F)); return 2; }
	if (c < 0x10000) {
		o[0] = (rx_u8)(0xE0 | (c >> 12)); o[1] = (rx_u8)(0x80 | ((c >> 6) & 0x3F));
		o[2] = (rx_u8)(0x80 | (c & 0x3F)); return 3;
	}
	o[0] = (rx_u8)(0xF0 | (c >> 18)); o[1] = (rx_u8)(0x80 | ((c >> 12) & 0x3F));
	o[2] = (rx_u8)(0x80 | ((c >> 6) & 0x3F)); o[3] = (rx_u8)(0x80 | (c & 0x3F));
	return 4;
}

/* ---- Case ------------------------------------------------------------- */

static int in(rx_u32 c, rx_u32 lo, rx_u32 hi) { return c >= lo && c <= hi; }

rx_u32 rx_lower(rx_u32 c, int utf8)
{
	if (in(c, 'A', 'Z')) return c + 32;
	if (c < 0xC0) return c;
	if (c <= 0xDE) return c == 0xD7 ? c : c + 32;
	if (!utf8 || c < 0x100) return c;
	if (c == 0x130) return 'i';
	if (c == 0x178) return 0xFF;
	if (in(c, 0x100, 0x137) && c != 0x130 && c != 0x131) return c | 1;
	if (in(c, 0x139, 0x148) || in(c, 0x179, 0x17E)) return (c & 1) ? c + 1 : c;
	if (in(c, 0x14A, 0x177)) return c | 1;
	if (in(c, 0x391, 0x3AB) && c != 0x3A2) return c + 32;
	if (c == 0x386) return 0x3AC;
	if (in(c, 0x388, 0x38A)) return c + 37;
	if (c == 0x38C) return 0x3CC;
	if (in(c, 0x38E, 0x38F)) return c + 63;
	if (in(c, 0x400, 0x40F)) return c + 80;
	if (in(c, 0x410, 0x42F)) return c + 32;
	if (in(c, 0x460, 0x481) || in(c, 0x48A, 0x4BF) || in(c, 0x4D0, 0x52F)) return c | 1;
	if (in(c, 0x4C1, 0x4CE)) return (c & 1) ? c + 1 : c;
	if (in(c, 0x531, 0x556)) return c + 48;
	if (in(c, 0x1E00, 0x1E95) || in(c, 0x1EA0, 0x1EFF)) return c | 1;
	if (in(c, 0xFF21, 0xFF3A)) return c + 32;
	return c;
}

rx_u32 rx_upper(rx_u32 c, int utf8)
{
	if (in(c, 'a', 'z')) return c - 32;
	if (c < 0xE0) return c;
	if (c <= 0xFE) return c == 0xF7 ? c : c - 32;
	if (!utf8) return c;
	if (c == 0xFF) return 0x178;
	if (c == 0x3C2) return 0x3A3;
	{
		/* the candidates rx_lower could have come from */
		static const rx_i32 d[] = { 1, -1, 32, 37, 63, 80, 48 };
		unsigned k;
		for (k = 0; k < sizeof d / sizeof d[0]; k++) {
			rx_u32 u = (rx_u32)((rx_i32)c - d[k]);
			if (u != c && u > 0xFF && rx_lower(u, 1) == c) return u;
		}
	}
	return c;
}

rx_u32 rx_fold(rx_u32 c, int utf8)
{
	if (utf8 && c == 0x3C2) return 0x3C3; /* final sigma */
	return rx_lower(c, utf8);
}

/* ---- Classes ---------------------------------------------------------- */

static int latin1_alpha(rx_u32 c)
{
	return in(c, 'A', 'Z') || in(c, 'a', 'z') || c == 0xAA || c == 0xB5 || c == 0xBA ||
	       (c >= 0xC0 && c <= 0xFF && c != 0xD7 && c != 0xF7);
}

static int big_alpha(rx_u32 c)
{
	if (c < 0x100) return latin1_alpha(c);
	if (c >= RX_INVALID_BASE) return 0;
	if (in(c, 0x100, 0x2AF)) return 1;
	if (in(c, 0x370, 0x3FF)) return c != 0x374 && c != 0x375 && c != 0x37E &&
	                                 c != 0x384 && c != 0x385 && c != 0x387;
	if (in(c, 0x400, 0x481) || in(c, 0x48A, 0x52F)) return 1;
	if (in(c, 0x531, 0x556) || in(c, 0x561, 0x587)) return 1;
	if (in(c, 0x5D0, 0x5EA) || in(c, 0x620, 0x64A) || in(c, 0x671, 0x6D3)) return 1;
	if (in(c, 0x904, 0x939) || in(c, 0xE01, 0xE30)) return 1;
	if (in(c, 0x10A0, 0x10FF) || in(c, 0x1100, 0x11FF)) return 1;
	if (in(c, 0x1E00, 0x1FFF)) return 1;
	if (in(c, 0x3041, 0x3096) || in(c, 0x30A1, 0x30FA)) return 1;
	if (in(c, 0x3400, 0x4DBF) || in(c, 0x4E00, 0x9FFF)) return 1;
	if (in(c, 0xAC00, 0xD7A3) || in(c, 0xF900, 0xFAFF)) return 1;
	if (in(c, 0xFF21, 0xFF3A) || in(c, 0xFF41, 0xFF5A)) return 1;
	if (in(c, 0x20000, 0x2FA1F)) return 1;
	return 0;
}

static int big_space(rx_u32 c, int utf8)
{
	if (c == ' ' || in(c, 9, 13)) return 1;
	if (!utf8) return 0; /* 0x85 and 0xA0 are spaces only as code points */
	return c == 0x85 || c == 0xA0 || c == 0x1680 || in(c, 0x2000, 0x200A) ||
	       c == 0x2028 || c == 0x2029 || c == 0x202F || c == 0x205F || c == 0x3000;
}

static int big_cntrl(rx_u32 c)
{
	return c < 32 || c == 127 || in(c, 0x80, 0x9F);
}

int rx_hasprop(rx_u32 c, rx_u32 props, int utf8)
{
	int alpha, digit, space, cntrl;
	if (c >= RX_INVALID_BASE) return 0;
	digit = in(c, '0', '9');
	alpha = big_alpha(c);
	if ((props & P_ALPHA) && alpha) return 1;
	if ((props & P_DIGIT) && digit) return 1;
	if ((props & P_ALNUM) && (alpha || digit)) return 1;
	if ((props & P_WORD) && (alpha || digit || c == '_')) return 1;
	if ((props & P_XDIGIT) && (digit || in(c, 'a', 'f') || in(c, 'A', 'F'))) return 1;
	if ((props & P_UPPER) && alpha && rx_lower(c, 1) != c) return 1;
	if ((props & P_LOWER) && alpha && (rx_upper(c, 1) != c || c == 0xDF || c == 0xAA ||
	    c == 0xBA || c == 0xB5)) return 1;
	space = big_space(c, utf8);
	if ((props & P_SPACE) && space) return 1;
	if ((props & P_BLANK) && (c == ' ' || c == '\t' || (utf8 && (c == 0xA0 || c == 0x1680 ||
	    in(c, 0x2000, 0x200A) || c == 0x202F || c == 0x205F || c == 0x3000)))) return 1;
	cntrl = big_cntrl(c);
	if ((props & P_CNTRL) && cntrl) return 1;
	if ((props & P_PRINT) && !cntrl && !(c >= 0x2028 && c <= 0x2029)) return 1;
	if ((props & P_GRAPH) && !cntrl && !space && c != 0xA0) return 1;
	if ((props & P_PUNCT) && !cntrl && !space && !alpha && !digit && c != 0xA0) return 1;
	return 0;
}

int rx_isword(rx_u32 c, int utf8)
{
	(void)utf8;
	if (c < 128) return in(c, 'a', 'z') || in(c, 'A', 'Z') || in(c, '0', '9') || c == '_';
	return big_alpha(c);
}
