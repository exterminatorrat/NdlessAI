/*
 * The expression engine: a parser and an integer-only evaluator shared by
 * graphing and by typed answers.
 *
 * Everything is Q16.16 fixed point, because the calculator's ARM926EJ-S has
 * no FPU and the device build rejects soft-float helpers. A value with no
 * real result saturates, which callers treat as "no value here".
 */
#include <stdint.h>
#include <string.h>

#include "expr.h"

#define Q16_PI     205887
#define Q16_2PI    411775
#define Q16_HALFPI 102944
#define Q16_E      178146
#define Q16_LN2    45426	/* ln 2 in Q16.16 */
#define Q24_LN2    11629073	/* ln 2 in Q24.24, for the logarithm series */
#define LN_Q       24		/* guard bits the logarithm series runs in */

q16_t gq_sat(int64_t v)
{
	if (v > Q16_MAX)
		return Q16_MAX;
	if (v < Q16_MIN)
		return Q16_MIN;
	return (q16_t)v;
}

q16_t gq_mul(q16_t a, q16_t b)
{
	return gq_sat(((int64_t)a * b) >> 16);
}

q16_t gq_div(q16_t a, q16_t b)
{
	if (b == 0)
		return a < 0 ? Q16_MIN : Q16_MAX;
	return gq_sat(((int64_t)a << 16) / b);
}

/*
 * sin via range reduction to [-pi/2, pi/2] and a Taylor series. The series
 * is accurate to well under one Q16.16 step over that interval, and the
 * chained multiplies keep every intermediate inside int32.
 */
q16_t gq_sin(q16_t x)
{
	int64_t r = (int64_t)x % Q16_2PI;
	q16_t t2, t3, t5, t7, t9;

	if (r > Q16_PI)
		r -= Q16_2PI;
	else if (r < -Q16_PI)
		r += Q16_2PI;
	if (r > Q16_HALFPI)
		r = Q16_PI - r;
	else if (r < -Q16_HALFPI)
		r = -Q16_PI - r;

	t2 = gq_mul((q16_t)r, (q16_t)r);
	t3 = gq_mul(t2, (q16_t)r);
	t5 = gq_mul(t3, t2);
	t7 = gq_mul(t5, t2);
	t9 = gq_mul(t7, t2);
	return (q16_t)(r - t3 / 6 + t5 / 120 - t7 / 5040 + t9 / 362880);
}

q16_t gq_cos(q16_t x)
{
	return gq_sin(gq_sat((int64_t)x + Q16_HALFPI));
}

q16_t gq_tan(q16_t x)
{
	q16_t s = gq_sin(x);
	q16_t c = gq_cos(x);

	if (c == 0)
		return s < 0 ? Q16_MIN : Q16_MAX;
	return gq_div(s, c);
}

/*
 * Integer square root of a 64-bit value: the largest r with r*r <= v.
 * Bitwise, so it needs no division and no float.
 */
static uint32_t isqrt64(uint64_t v)
{
	uint64_t r = 0;
	uint64_t bit = (uint64_t)1 << 62;

	while (bit > v)
		bit >>= 2;
	while (bit) {
		if (v >= r + bit) {
			v -= r + bit;
			r = (r >> 1) + bit;
		} else {
			r >>= 1;
		}
		bit >>= 2;
	}
	return (uint32_t)r;
}

/* Square root of a non-negative Q16.16 value; a negative input has no value. */
static q16_t gq_sqrt(q16_t x)
{
	if (x < 0)
		return Q16_MAX;
	return (q16_t)isqrt64((uint64_t)(uint32_t)x << 16);
}

/*
 * Natural logarithm: reduce to [1, 2) by powers of two, then the atanh series
 * ln(m) = 2(t + t^3/3 + t^5/5 + ...) with t = (m-1)/(m+1) <= 1/3. The series
 * runs in Q24 rather than Q16, because t and every truncated multiply cost
 * about a Q16 step each and that showed up in the third significant digit of
 * ln near 1. Eight guard bits put the error below one Q16 step over the whole
 * domain.
 */
static q16_t gq_ln(q16_t x)
{
	int32_t k = 0;
	int64_t m, t, t2, term, sum;
	int i;

	if (x <= 0)
		return Q16_MAX;
	while (x < Q16_ONE && k > -40) {
		x = (q16_t)((uint32_t)x << 1);
		k--;
	}
	while (x >= 2 * Q16_ONE && k < 40) {
		x = (q16_t)((uint32_t)x >> 1);
		k++;
	}
	m = x;
	t = ((m - Q16_ONE) << LN_Q) / (m + Q16_ONE);
	t2 = (t * t) >> LN_Q;
	sum = 0;
	term = t;
	for (i = 1; i <= 11; i += 2) {
		sum += term / i;
		term = (term * t2) >> LN_Q;
	}
	/* The series gives ln(m)/2, so one extra bit of shift restores the factor. */
	return gq_sat((sum >> (LN_Q - 17)) + (int64_t)k * Q24_LN2 / 256);
}

/*
 * Exponential: x = k ln2 + r with |r| <= ln2/2, a Taylor series for e^r, then
 * scale by 2^k. Saturates rather than overflowing, and never calls libm.
 */
static q16_t gq_exp(q16_t x)
{
	int32_t k = x / Q16_LN2;
	q16_t r = gq_sat((int64_t)x - (int64_t)k * Q16_LN2);
	q16_t sum = Q16_ONE;
	q16_t term = Q16_ONE;
	int i;

	for (i = 1; i <= 9; i++) {
		term = gq_mul(term, r) / i;
		sum = gq_sat((int64_t)sum + term);
	}
	if (k > 15)
		return Q16_MAX;
	if (k < -16)
		return 0;
	if (k >= 0)
		return gq_sat((int64_t)sum << k);
	return (q16_t)((int64_t)sum >> (-k));
}

typedef struct {
	const char *p;
	gexpr_t *e;
	int err;
} gp_t;

static const char *const gerr[] = {
	"",
	"Expression is too complex.",
	"Missing a closing bracket.",
	"Unexpected character.",
	"Malformed number.",
	"Unknown function.",
	"Expected a bracket after the function.",
	"Enter a function of x.",
	"Unexpected text after the expression.",
};

static void skip(gp_t *g)
{
	while (*g->p == ' ' || *g->p == '\t')
		g->p++;
}

static int node(gp_t *g, int op, int a, int b, int32_t num)
{
	int i;

	if (g->e->n >= EXPR_MAX_NODES) {
		g->err = 1;
		return -1;
	}
	i = g->e->n++;
	g->e->node[i].op = (int16_t)op;
	g->e->node[i].a = (int16_t)a;
	g->e->node[i].b = (int16_t)b;
	g->e->node[i].num = num;
	return i;
}

static int parse_expr(gp_t *g);
static int parse_unary(gp_t *g);

static int parse_number(gp_t *g)
{
	int64_t ip = 0, frac = 0, v;
	int fdig = 0, any = 0;

	while (*g->p >= '0' && *g->p <= '9') {
		if (ip < 100000)
			ip = ip * 10 + (*g->p - '0');
		g->p++;
		any = 1;
	}
	if (*g->p == '.') {
		g->p++;
		while (*g->p >= '0' && *g->p <= '9') {
			if (fdig < 9) {
				frac = frac * 10 + (*g->p - '0');
				fdig++;
			}
			g->p++;
			any = 1;
		}
	}
	if (!any) {
		g->err = 4;
		return -1;
	}
	v = ip << 16;
	if (fdig) {
		int64_t den = 1;

		for (int i = 0; i < fdig; i++)
			den *= 10;
		v += (frac << 16) / den;
	}
	return node(g, G_NUM, 0, 0, gq_sat(v));
}

static int parse_func(gp_t *g)
{
	int op, a;

	if (g->p[0] == 's' && g->p[1] == 'i' && g->p[2] == 'n') {
		op = G_SIN;
		g->p += 3;
	} else if (g->p[0] == 's' && g->p[1] == 'q' && g->p[2] == 'r' &&
	           g->p[3] == 't') {
		op = G_SQRT;
		g->p += 4;
	} else if (g->p[0] == 'c' && g->p[1] == 'o' && g->p[2] == 's') {
		op = G_COS;
		g->p += 3;
	} else if (g->p[0] == 't' && g->p[1] == 'a' && g->p[2] == 'n') {
		op = G_TAN;
		g->p += 3;
	} else if (g->p[0] == 'a' && g->p[1] == 'b' && g->p[2] == 's') {
		op = G_ABS;
		g->p += 3;
	} else if (g->p[0] == 'l' && g->p[1] == 'n') {
		op = G_LN;
		g->p += 2;
	} else if (g->p[0] == 'e' && g->p[1] == 'x' && g->p[2] == 'p') {
		op = G_EXP;
		g->p += 3;
	} else {
		g->err = 5;
		return -1;
	}
	skip(g);
	if (*g->p != '(') {
		g->err = 6;
		return -1;
	}
	g->p++;
	a = parse_expr(g);
	if (a < 0)
		return -1;
	skip(g);
	if (*g->p != ')') {
		g->err = 2;
		return -1;
	}
	g->p++;
	return node(g, op, a, 0, 0);
}

static int parse_atom(gp_t *g)
{
	char c;

	skip(g);
	c = *g->p;
	if (c == '(') {
		int a;

		g->p++;
		a = parse_expr(g);
		if (a < 0)
			return -1;
		skip(g);
		if (*g->p != ')') {
			g->err = 2;
			return -1;
		}
		g->p++;
		return a;
	}
	if (c == 'x') {
		g->p++;
		return node(g, G_X, 0, 0, 0);
	}
	/* y exists only for differential equations. Every other path checks
	 * gexpr_has_y and refuses, so a stray y cannot quietly evaluate as 0. */
	if (c == 'y') {
		g->p++;
		return node(g, G_Y, 0, 0, 0);
	}
	if ((c >= '0' && c <= '9') || c == '.')
		return parse_number(g);
	if (c == 'p') {
		if (g->p[1] != 'i') {
			g->err = 3;
			return -1;
		}
		g->p += 2;
		return node(g, G_NUM, 0, 0, Q16_PI);
	}
	if (c == 'e' && !(g->p[1] == 'x' && g->p[2] == 'p')) {
		g->p++;
		return node(g, G_NUM, 0, 0, Q16_E);
	}
	if (c == 's' || c == 'c' || c == 't' || c == 'a' || c == 'l' || c == 'e')
		return parse_func(g);
	g->err = 3;
	return -1;
}

static int parse_power(gp_t *g)
{
	int a = parse_atom(g);

	if (a < 0)
		return -1;
	skip(g);
	if (*g->p == '^') {
		int b;

		g->p++;
		b = parse_unary(g);
		if (b < 0)
			return -1;
		return node(g, G_POW, a, b, 0);
	}
	return a;
}

static int parse_unary(gp_t *g)
{
	int a;

	skip(g);
	if (*g->p == '-') {
		g->p++;
		a = parse_unary(g);
		if (a < 0)
			return -1;
		return node(g, G_NEG, a, 0, 0);
	}
	if (*g->p == '+') {
		g->p++;
		return parse_unary(g);
	}
	return parse_power(g);
}

static int starts_atom(char c)
{
	return (c >= '0' && c <= '9') || c == '.' || c == 'x' || c == 'y' ||
	       c == '(' ||
	       c == 's' || c == 'c' || c == 't' || c == 'a' || c == 'l' ||
	       c == 'e' || c == 'p';
}

static int parse_term(gp_t *g)
{
	int a = parse_unary(g);

	if (a < 0)
		return -1;
	for (;;) {
		char c;
		int op, b;

		skip(g);
		c = *g->p;
		if (c == '*') {
			op = G_MUL;
			g->p++;
		} else if (c == '/') {
			op = G_DIV;
			g->p++;
		} else if (starts_atom(c)) {
			op = G_MUL;
		} else {
			break;
		}
		b = parse_unary(g);
		if (b < 0)
			return -1;
		a = node(g, op, a, b, 0);
		if (a < 0)
			return -1;
	}
	return a;
}

static int parse_expr(gp_t *g)
{
	int a = parse_term(g);

	if (a < 0)
		return -1;
	for (;;) {
		char c;
		int b;

		skip(g);
		c = *g->p;
		if (c != '+' && c != '-')
			break;
		g->p++;
		b = parse_term(g);
		if (b < 0)
			return -1;
		a = node(g, c == '+' ? G_ADD : G_SUB, a, b, 0);
		if (a < 0)
			return -1;
	}
	return a;
}

int gexpr_parse(const char *text, gexpr_t *e, char *err, int errlen)
{
	gp_t g;
	int r;

	e->n = 0;
	e->root = -1;
	if (err && errlen > 0)
		err[0] = 0;

	g.p = text;
	g.e = e;
	g.err = 0;
	skip(&g);
	if (!*g.p) {
		g.err = 7;
		goto fail;
	}
	r = parse_expr(&g);
	if (r < 0)
		goto fail;
	skip(&g);
	if (*g.p) {
		g.err = 8;
		goto fail;
	}
	e->root = r;
	return 0;

fail:
	if (err && errlen > 0) {
		const char *m = gerr[g.err];
		int i = 0;

		while (m[i] && i < errlen - 1) {
			err[i] = m[i];
			i++;
		}
		err[i] = 0;
	}
	return -1;
}

/* ---- evaluation ---- */

/*
 * Power. An integer exponent in [-8, 8] is repeated multiplication, which is
 * exact; anything else goes through exp(ex * ln b). A fractional exponent is
 * real only for a positive base, and a negative base keeps the sign of an odd
 * integer exponent.
 */
static q16_t qpow(q16_t b, q16_t ex)
{
	int32_t k;
	int n;
	q16_t r;

	if ((ex & 0xFFFF) == 0) {
		k = ex >> 16;
		if (b == 0)
			return k > 0 ? 0 : Q16_MAX;
		if (k >= -8 && k <= 8) {
			n = k < 0 ? -k : k;
			r = Q16_ONE;
			for (int i = 0; i < n; i++)
				r = gq_mul(r, b);
			if (k < 0)
				r = gq_div(Q16_ONE, r);
			return r;
		}
		if (b < 0) {
			r = gq_exp(gq_mul(ex, gq_ln((q16_t)-b)));
			return (k & 1) ? gq_sat(-(int64_t)r) : r;
		}
		return gq_exp(gq_mul(ex, gq_ln(b)));
	}
	if (b == 0)
		return ex > 0 ? 0 : Q16_MAX;
	if (b < 0)
		return Q16_MAX;
	return gq_exp(gq_mul(ex, gq_ln(b)));
}

typedef struct {
	q16_t x;
	q16_t y;
} gvars_t;

static q16_t eval_node(const gexpr_t *e, int i, gvars_t v)
{
	const gnode_t *n = &e->node[i];

	switch (n->op) {
	case G_NUM:
		return n->num;
	case G_X:
		return v.x;
	case G_Y:
		return v.y;
	case G_ADD:
		return gq_sat((int64_t)eval_node(e, n->a, v) + eval_node(e, n->b, v));
	case G_SUB:
		return gq_sat((int64_t)eval_node(e, n->a, v) - eval_node(e, n->b, v));
	case G_MUL:
		return gq_mul(eval_node(e, n->a, v), eval_node(e, n->b, v));
	case G_DIV:
		return gq_div(eval_node(e, n->a, v), eval_node(e, n->b, v));
	case G_POW:
		return qpow(eval_node(e, n->a, v), eval_node(e, n->b, v));
	case G_SIN:
		return gq_sin(eval_node(e, n->a, v));
	case G_COS:
		return gq_cos(eval_node(e, n->a, v));
	case G_TAN:
		return gq_tan(eval_node(e, n->a, v));
	case G_NEG:
		return gq_sat(-(int64_t)eval_node(e, n->a, v));
	case G_SQRT:
		return gq_sqrt(eval_node(e, n->a, v));
	case G_ABS: {
		q16_t a = eval_node(e, n->a, v);

		return a < 0 ? gq_sat(-(int64_t)a) : a;
	}
	case G_LN:
		return gq_ln(eval_node(e, n->a, v));
	case G_EXP:
		return gq_exp(eval_node(e, n->a, v));
	default:
		return 0;
	}
}

q16_t gexpr_eval(const gexpr_t *e, q16_t x)
{
	gvars_t v;

	v.x = x;
	v.y = 0;
	if (e->root < 0)
		return 0;
	return eval_node(e, e->root, v);
}

q16_t gexpr_eval2(const gexpr_t *e, q16_t x, q16_t y)
{
	gvars_t v;

	v.x = x;
	v.y = y;
	if (e->root < 0)
		return 0;
	return eval_node(e, e->root, v);
}

int gexpr_has_y(const gexpr_t *e)
{
	if (e->root < 0)
		return 0;
	for (int i = 0; i < e->n; i++)
		if (e->node[i].op == G_Y)
			return 1;
	return 0;
}

int gexpr_has_x(const gexpr_t *e)
{
	if (e->root < 0)
		return 0;
	for (int i = 0; i < e->n; i++)
		if (e->node[i].op == G_X)
			return 1;
	return 0;
}

/*
 * Renders a value to four significant digits, matching how the solvers print
 * answers. The graph's own fmt_q16 is deliberately different: axis labels want
 * a fixed number of decimals, not significant digits.
 */
int gexpr_format(q16_t v, char *out, int outlen)
{
	static const int64_t p10[] = { 1, 10, 100, 1000, 10000, 100000,
	                               1000000, 10000000, 100000000 };
	int64_t a = v < 0 ? -(int64_t)v : v;
	char digs[24];
	char line[40];
	int d = 0, nd = 0, i, p = 0, ip;
	int64_t n;

	if (outlen <= 0)
		return -1;
	if (a == 0) {
		if (outlen < 2)
			return -1;
		out[0] = '0';
		out[1] = 0;
		return 0;
	}
	/* Enough decimals that the scaled value keeps four significant digits. */
	for (d = 0; d <= 8; d++) {
		n = (a * p10[d] + 32768) >> 16;
		if (n >= 1000)
			break;
	}
	n = (a * p10[d] + 32768) >> 16;
	do {
		digs[nd++] = (char)('0' + (int)(n % 10));
		n /= 10;
	} while (n > 0 && nd < (int)sizeof digs);
	while (nd <= d)
		digs[nd++] = '0';
	/* digs[0] is the last fractional digit, so leading zeros here are
	 * trailing zeros in the answer. */
	while (d > 0 && digs[0] == '0') {
		for (i = 1; i < nd; i++)
			digs[i - 1] = digs[i];
		nd--;
		d--;
	}
	if (v < 0)
		line[p++] = '-';
	ip = nd - d;
	if (ip <= 0) {
		line[p++] = '0';
		if (d > 0) {
			line[p++] = '.';
			for (i = 0; i < d - nd; i++)
				line[p++] = '0';
			for (i = nd - 1; i >= 0; i--)
				line[p++] = digs[i];
		}
	} else {
		for (i = nd - 1; i >= d; i--)
			line[p++] = digs[i];
		if (d > 0) {
			line[p++] = '.';
			for (i = d - 1; i >= 0; i--)
				line[p++] = digs[i];
		}
	}
	line[p] = 0;
	if (p + 1 > outlen)
		return -1;
	memcpy(out, line, (size_t)p + 1);
	return 0;
}

/* ---- differentiation ---- */

/*
 * A derivative is built against the source: the source's nodes stay as out's
 * prefix, so a rule points at the original subtree instead of copying it.
 * Only constructed nodes are appended, and every constructor simplifies before
 * it appends, so the tree never holds the 0*a and a*1 that the rules would
 * otherwise leave behind.
 *
 * The memo table means a subtree reached twice is derived once. Trees can be
 * DAGs on the way in, because a derivative is itself a readable source.
 */
typedef struct {
	gexpr_t *o;
	int err;
	int memo[EXPR_MAX_NODES];
} dv_t;

static int gnew(dv_t *d, int op, int a, int b, int32_t num)
{
	gexpr_t *o = d->o;
	int i;

	if (d->err)
		return -1;
	if (o->n >= EXPR_MAX_NODES) {
		d->err = GEXPR_DIFF_TOO_BIG;
		return -1;
	}
	i = o->n++;
	o->node[i].op = (int16_t)op;
	o->node[i].a = (int16_t)a;
	o->node[i].b = (int16_t)b;
	o->node[i].num = num;
	return i;
}

static int gnum(dv_t *d, int32_t v)
{
	return gnew(d, G_NUM, 0, 0, v);
}

static int gnum_is(const gexpr_t *o, int i)
{
	return i >= 0 && o->node[i].op == G_NUM;
}

static int32_t gnum_of(const gexpr_t *o, int i)
{
	return o->node[i].num;
}

static int gneg(dv_t *d, int a)
{
	gexpr_t *o = d->o;

	if (d->err || a < 0)
		return -1;
	if (o->node[a].op == G_NUM)
		return gnum(d, (int32_t)(-(int64_t)o->node[a].num));
	if (o->node[a].op == G_NEG)
		return o->node[a].a;
	return gnew(d, G_NEG, a, 0, 0);
}

static int gadd(dv_t *d, int a, int b)
{
	gexpr_t *o = d->o;

	if (d->err || a < 0 || b < 0)
		return -1;
	if (gnum_is(o, a) && gnum_of(o, a) == 0)
		return b;
	if (gnum_is(o, b) && gnum_of(o, b) == 0)
		return a;
	if (gnum_is(o, a) && gnum_is(o, b))
		return gnum(d, gq_sat((int64_t)gnum_of(o, a) + gnum_of(o, b)));
	return gnew(d, G_ADD, a, b, 0);
}

static int gsub(dv_t *d, int a, int b)
{
	gexpr_t *o = d->o;

	if (d->err || a < 0 || b < 0)
		return -1;
	if (gnum_is(o, b) && gnum_of(o, b) == 0)
		return a;
	if (gnum_is(o, a) && gnum_of(o, a) == 0)
		return gneg(d, b);
	if (gnum_is(o, a) && gnum_is(o, b))
		return gnum(d, gq_sat((int64_t)gnum_of(o, a) - gnum_of(o, b)));
	return gnew(d, G_SUB, a, b, 0);
}

static int gmul(dv_t *d, int a, int b)
{
	gexpr_t *o = d->o;

	if (d->err || a < 0 || b < 0)
		return -1;
	if (gnum_is(o, a) && gnum_of(o, a) == 0)
		return gnum(d, 0);
	if (gnum_is(o, b) && gnum_of(o, b) == 0)
		return gnum(d, 0);
	if (gnum_is(o, a) && gnum_of(o, a) == Q16_ONE)
		return b;
	if (gnum_is(o, b) && gnum_of(o, b) == Q16_ONE)
		return a;
	if (gnum_is(o, a) && gnum_is(o, b))
		return gnum(d, gq_mul(gnum_of(o, a), gnum_of(o, b)));
	/* A negated factor moves out in front of the product, so a sum of terms
	 * prints as 2xcos(x)-x^2sin(x) instead of 2xcos(x)+-x^2sin(x). */
	if (o->node[b].op == G_NEG)
		return gneg(d, gmul(d, a, o->node[b].a));
	if (o->node[a].op == G_NEG)
		return gneg(d, gmul(d, o->node[a].a, b));
	/* The number goes first: 3exp(3x) reads better than exp(3x)*3, and the
	 * product is bit-identical either way because gq_mul is symmetric. */
	if (gnum_is(o, b) && !gnum_is(o, a))
		return gmul(d, b, a);
	/* Two numeric factors separated by a product collapse into one node, so
	 * 2*(3x^2) prints as 6x^2 and takes a node less. */
	if (gnum_is(o, a) && o->node[b].op == G_MUL && gnum_is(o, o->node[b].a))
		return gmul(d, gnum(d, gq_mul(gnum_of(o, a), gnum_of(o, o->node[b].a))),
		            o->node[b].b);
	if (gnum_is(o, b) && o->node[a].op == G_MUL && gnum_is(o, o->node[a].a))
		return gmul(d, gnum(d, gq_mul(gnum_of(o, b), gnum_of(o, o->node[a].a))),
		            o->node[a].b);
	return gnew(d, G_MUL, a, b, 0);
}

static int gdiv(dv_t *d, int a, int b)
{
	gexpr_t *o = d->o;

	if (d->err || a < 0 || b < 0)
		return -1;
	if (gnum_is(o, a) && gnum_of(o, a) == 0)
		return gnum(d, 0);
	if (gnum_is(o, b) && gnum_of(o, b) == Q16_ONE)
		return a;
	if (gnum_is(o, a) && gnum_is(o, b) && gnum_of(o, b) != 0)
		return gnum(d, gq_div(gnum_of(o, a), gnum_of(o, b)));
	return gnew(d, G_DIV, a, b, 0);
}

static int gpow(dv_t *d, int a, int b)
{
	gexpr_t *o = d->o;

	if (d->err || a < 0 || b < 0)
		return -1;
	if (gnum_is(o, b) && gnum_of(o, b) == 0)
		return gnum(d, Q16_ONE);
	if (gnum_is(o, b) && gnum_of(o, b) == Q16_ONE)
		return a;
	if (gnum_is(o, a) && gnum_of(o, a) == Q16_ONE)
		return gnum(d, Q16_ONE);
	if (gnum_is(o, a) && gnum_is(o, b))
		return gnum(d, qpow(gnum_of(o, a), gnum_of(o, b)));
	return gnew(d, G_POW, a, b, 0);
}

/* Whether the subtree at i depends on x; a constant subtree derives to 0. */
static int node_has_x(const gexpr_t *f, int i)
{
	const gnode_t *n;

	if (i < 0)
		return 0;
	n = &f->node[i];
	switch (n->op) {
	case G_X:
		return 1;
	case G_NUM:
		return 0;
	case G_ADD:
	case G_SUB:
	case G_MUL:
	case G_DIV:
	case G_POW:
		return node_has_x(f, n->a) || node_has_x(f, n->b);
	default:
		return node_has_x(f, n->a);
	}
}

static int d_node(dv_t *d, const gexpr_t *f, int i)
{
	const gnode_t *n;
	int r, da;

	if (i < 0 || d->err)
		return -1;
	if (d->memo[i] >= 0)
		return d->memo[i];
	if (!node_has_x(f, i)) {
		d->memo[i] = gnum(d, 0);
		return d->memo[i];
	}
	n = &f->node[i];
	switch (n->op) {
	case G_X:
		r = gnum(d, Q16_ONE);
		break;
	case G_ADD:
		r = gadd(d, d_node(d, f, n->a), d_node(d, f, n->b));
		break;
	case G_SUB:
		r = gsub(d, d_node(d, f, n->a), d_node(d, f, n->b));
		break;
	case G_MUL:
		r = gadd(d, gmul(d, d_node(d, f, n->a), n->b),
		         gmul(d, n->a, d_node(d, f, n->b)));
		break;
	case G_DIV:
		/*
		 * A constant denominator needs no quotient rule: (u/c)' is u'/c.
		 * The full rule multiplies through by the denominator twice and
		 * squares it, which both costs nodes and overflows Q16 earlier
		 * than the answer does, so x^6/6 would not survive the trip.
		 */
		if (!node_has_x(f, n->b)) {
			r = gdiv(d, d_node(d, f, n->a), n->b);
			break;
		}
		/* (u'v - uv') / v^2 */
		r = gdiv(d, gsub(d, gmul(d, d_node(d, f, n->a), n->b),
		                 gmul(d, n->a, d_node(d, f, n->b))),
		         gpow(d, n->b, gnum(d, 2 * Q16_ONE)));
		break;
	case G_POW:
		/* A constant exponent keeps the power rule: (u^c)' = c u^(c-1) u'.
		 * An exponent that depends on x needs a logarithm the caller has
		 * not been promised, so it is refused rather than approximated. */
		if (node_has_x(f, n->b)) {
			d->err = GEXPR_DIFF_UNSUPPORTED;
			return -1;
		}
		da = d_node(d, f, n->a);
		r = gmul(d, gmul(d, n->b,
		                 gpow(d, n->a, gsub(d, n->b, gnum(d, Q16_ONE)))), da);
		break;
	case G_NEG:
		r = gneg(d, d_node(d, f, n->a));
		break;
	case G_SIN:
		r = gmul(d, gnew(d, G_COS, n->a, 0, 0), d_node(d, f, n->a));
		break;
	case G_COS:
		r = gneg(d, gmul(d, gnew(d, G_SIN, n->a, 0, 0), d_node(d, f, n->a)));
		break;
	case G_TAN:
		r = gmul(d, gdiv(d, gnum(d, Q16_ONE),
		                 gpow(d, gnew(d, G_COS, n->a, 0, 0),
		                      gnum(d, 2 * Q16_ONE))),
		         d_node(d, f, n->a));
		break;
	case G_EXP:
		/* The node itself is the chain's outer factor. */
		r = gmul(d, i, d_node(d, f, n->a));
		break;
	case G_LN:
		r = gdiv(d, d_node(d, f, n->a), n->a);
		break;
	case G_SQRT:
		r = gdiv(d, d_node(d, f, n->a),
		         gmul(d, gnum(d, 2 * Q16_ONE), gnew(d, G_SQRT, n->a, 0, 0)));
		break;
	case G_ABS:
		/* |u|' = u/|u| * u', the sign of u wherever u is not zero. */
		r = gmul(d, gdiv(d, n->a, gnew(d, G_ABS, n->a, 0, 0)),
		         d_node(d, f, n->a));
		break;
	default:
		d->err = GEXPR_DIFF_UNSUPPORTED;
		return -1;
	}
	d->memo[i] = r;
	return r;
}

int gexpr_derive(const gexpr_t *f, gexpr_t *out)
{
	dv_t d;
	int r, i, root;

	if (!f || !out || f->root < 0 || f->root >= f->n)
		return GEXPR_DIFF_UNSUPPORTED;
	if (f->n > EXPR_MAX_NODES)
		return GEXPR_DIFF_TOO_BIG;
	/* out may be f, and clearing the root is how a refused derivative stays
	 * unreadable, so the source root is kept here. */
	root = f->root;
	out->root = -1;
	if (out != f) {
		for (i = 0; i < f->n; i++)
			out->node[i] = f->node[i];
		out->n = f->n;
	}
	d.o = out;
	d.err = 0;
	for (i = 0; i < f->n; i++)
		d.memo[i] = -1;
	r = d_node(&d, f, root);
	if (r < 0)
		return d.err ? d.err : GEXPR_DIFF_UNSUPPORTED;
	out->root = r;
	return GEXPR_DIFF_OK;
}

/* ---- symbolic integration ---- */

/*
 * Antiderivatives, as expressions rather than numbers. The rules are the ones
 * a Calc AB paper needs: powers, sums, constant multiples, 1/x, and sin, cos
 * and exp of a linear argument. Anything else is refused (-1) rather than
 * guessed, because a wrong antiderivative differentiates into something that
 * is nearly the integrand and looks right.
 *
 * The output is exact where the input is: x^2 integrates to x^3/3 as a
 * division node, not to a Q16 coefficient of 0.3333, so printing and
 * differentiating the answer both stay exact. Evaluating it at a point is
 * still fixed point, like everything else here.
 */
static int gx_node(dv_t *d)
{
	return gnew(d, G_X, 0, 0, 0);
}

/* The factor a linear argument is multiplied by: x counts as 1. */
static int linear_scale(const gexpr_t *f, int i)
{
	if (i < 0)
		return -1;
	if (f->node[i].op == G_X)
		return Q16_ONE;
	if (f->node[i].op == G_MUL && gnum_is(f, f->node[i].a) &&
	    f->node[f->node[i].b].op == G_X) {
		int32_t k = gnum_of(f, f->node[i].a);

		return k > 0 ? k : -1;
	}
	return -1;
}

/*
 * The exponent of a bare x^k subtree, in Q16 units, or INT32_MIN when the
 * subtree is not a power of x alone.
 */
static int32_t pow_exponent(const gexpr_t *f, int i)
{
	if (i < 0)
		return INT32_MIN;
	if (f->node[i].op == G_X)
		return Q16_ONE;
	if (f->node[i].op == G_POW && f->node[f->node[i].a].op == G_X &&
	    gnum_is(f, f->node[i].b))
		return gnum_of(f, f->node[i].b);
	return INT32_MIN;
}

/*
 * coef * x^e, integrated. The coefficient division is kept exact where it can
 * be: 3x^2 integrates to x^3 rather than to 1.0000x^3, and 2x to x^2 rather
 * than to 2(x^2/2), because a student reads the printed form.
 */
static int integrate_pow(dv_t *d, int32_t coef, int32_t e)
{
	int64_t e1 = (int64_t)e + Q16_ONE;
	int p;

	if (e1 == 0) {
		int ln = gnew(d, G_LN, gx_node(d), 0, 0);

		if (ln < 0)
			return -1;
		return coef == Q16_ONE ? ln : gmul(d, gnum(d, coef), ln);
	}
	p = gpow(d, gx_node(d), gnum(d, (int32_t)e1));
	if (p < 0)
		return -1;
	/* Both divisions below are exact in Q16: e1 is an integer count of
	 * Q16 units, and a coefficient that divides by it gives an integer. */
	if (coef % e1 == 0)
		return gmul(d, gnum(d, (int32_t)((int64_t)coef * Q16_ONE / e1)), p);
	if (coef == Q16_ONE)
		return gdiv(d, p, gnum(d, (int32_t)e1));
	return gdiv(d, gmul(d, gnum(d, coef), p), gnum(d, (int32_t)e1));
}

static int i_node(dv_t *d, const gexpr_t *f, int i)
{
	const gnode_t *n;
	int32_t e;

	if (d->err || i < 0)
		return -1;
	if (!node_has_x(f, i))
		return gmul(d, i, gx_node(d));

	n = &f->node[i];
	switch (n->op) {
	case G_X:
		return integrate_pow(d, Q16_ONE, Q16_ONE);
	case G_ADD:
		return gadd(d, i_node(d, f, n->a), i_node(d, f, n->b));
	case G_SUB:
		return gsub(d, i_node(d, f, n->a), i_node(d, f, n->b));
	case G_NEG:
		return gneg(d, i_node(d, f, n->a));
	case G_MUL:
		/* A numeric factor over a power of x is the common shape:
		 * 3x^2, 2x. Anything else in a product needs expanding first. */
		if (gnum_is(f, n->a) && (e = pow_exponent(f, n->b)) != INT32_MIN)
			return integrate_pow(d, gnum_of(f, n->a), e);
		if (gnum_is(f, n->b) && (e = pow_exponent(f, n->a)) != INT32_MIN)
			return integrate_pow(d, gnum_of(f, n->b), e);
		if (!node_has_x(f, n->a))
			return gmul(d, n->a, i_node(d, f, n->b));
		if (!node_has_x(f, n->b))
			return gmul(d, n->b, i_node(d, f, n->a));
		return -1;
	case G_DIV:
		if (!node_has_x(f, n->b))
			return gdiv(d, i_node(d, f, n->a), n->b);
		/* c / x^k is c * x^-k, which is where 1/x comes from. */
		if (!node_has_x(f, n->a) &&
		    (e = pow_exponent(f, n->b)) != INT32_MIN && e != 0)
			return integrate_pow(d, gnum_is(f, n->a) ? gnum_of(f, n->a)
			                                         : Q16_ONE,
			                     -e);
		return -1;
	case G_POW:
		if ((e = pow_exponent(f, i)) == INT32_MIN)
			return -1;
		return integrate_pow(d, Q16_ONE, e);
	case G_SIN:
	case G_COS:
	case G_EXP: {
		int k = linear_scale(f, n->a);
		int r;

		if (k <= 0)
			return -1;
		if (n->op == G_SIN)
			r = gneg(d, gnew(d, G_COS, n->a, 0, 0));
		else if (n->op == G_COS)
			r = gnew(d, G_SIN, n->a, 0, 0);
		else
			r = gnew(d, G_EXP, n->a, 0, 0);
		if (r < 0)
			return -1;
		return k == Q16_ONE ? r : gdiv(d, r, gnum(d, k));
	}
	default:
		return -1;
	}
}

int gexpr_integrate_symbolic(const gexpr_t *f, gexpr_t *out)
{
	dv_t d;
	int i, root, r;

	if (!f || !out || out == f || f->root < 0 || f->root >= f->n)
		return GEXPR_DIFF_UNSUPPORTED;
	if (f->n > EXPR_MAX_NODES)
		return GEXPR_DIFF_TOO_BIG;
	root = f->root;
	out->root = -1;
	for (i = 0; i < f->n; i++)
		out->node[i] = f->node[i];
	out->n = f->n;

	d.o = out;
	d.err = 0;
	for (i = 0; i < f->n; i++)
		d.memo[i] = -1;
	r = i_node(&d, f, root);
	if (r < 0)
		return d.err ? d.err : GEXPR_DIFF_UNSUPPORTED;
	out->root = r;
	return GEXPR_DIFF_OK;
}

/* ---- Taylor series ---- */

/*
 * The Taylor polynomial of f about a, with `terms` coefficients. A series is
 * only worth printing if the printed form is also an expression the parser
 * accepts, so it is built as a tree like everything else: the coefficients are
 * f^(k)(a)/k! evaluated in Q16, and the powers share one (x-a) subtree.
 *
 * The coefficients are fixed point, so a printed series is an approximation of
 * an approximation - the test compares values rather than text.
 */
int gexpr_taylor(const gexpr_t *f, q16_t a, int terms, gexpr_t *out)
{
	dv_t d;
	gexpr_t cur, der;
	int i, sum, k;
	int64_t fact = 1;

	if (!f || !out || out == f || terms < 1 || terms > EXPR_MAX_TAYLOR_TERMS)
		return GEXPR_DIFF_UNSUPPORTED;
	if (f->n > EXPR_MAX_NODES)
		return GEXPR_DIFF_TOO_BIG;

	out->root = -1;
	for (i = 0; i < f->n; i++)
		out->node[i] = f->node[i];
	out->n = f->n;

	d.o = out;
	d.err = 0;
	for (i = 0; i < f->n; i++)
		d.memo[i] = -1;

	sum = gnum(&d, gexpr_eval(f, a));
	if (sum < 0)
		return d.err ? d.err : GEXPR_DIFF_UNSUPPORTED;

	cur = *f;
	for (k = 1; k < terms; k++) {
		int32_t coef, sub, pw, term;
		int rc = gexpr_derive(&cur, &der);

		if (rc != GEXPR_DIFF_OK)
			return rc;
		fact *= k;
		coef = (int32_t)(gexpr_eval(&der, a) / fact);

		sub = gsub(&d, gx_node(&d), gnum(&d, a));
		pw = gpow(&d, sub, gnum(&d, gq_sat((int64_t)k << 16)));
		term = gmul(&d, gnum(&d, coef), pw);
		sum = gadd(&d, sum, term);
		if (sum < 0)
			return d.err ? d.err : GEXPR_DIFF_UNSUPPORTED;
		cur = der;
	}
	out->root = sum;
	return GEXPR_DIFF_OK;
}

/* ---- differential equations ---- */

/*
 * y' = f(x,y), stepped with classic RK4 at a fixed step. Fourth order is the
 * point of doing it numerically at all: a student's answer wants four
 * significant digits, and Euler at a step small enough for that would need
 * thousands of steps on a 132 MHz processor.
 *
 * The table is written into ys when it is supplied: steps + 1 values, y[0]
 * being the initial condition. x is not stored, because the step is uniform
 * and x0 is the caller's.
 */
int gexpr_rk4(const gexpr_t *f, q16_t x0, q16_t y0, q16_t xn, int steps,
              q16_t *ys, int ys_max)
{
	q16_t x, y;
	q16_t h, half;
	int i;

	if (!f || f->root < 0 || steps < 1 || steps > EXPR_MAX_ODE_STEPS)
		return GEXPR_DIFF_UNSUPPORTED;
	if (ys && ys_max < steps + 1)
		return GEXPR_DIFF_UNSUPPORTED;

	h = gq_sat(((int64_t)(xn - x0)) / steps);
	if (h == 0)
		return GEXPR_DIFF_UNSUPPORTED;
	half = h >> 1;

	x = x0;
	y = y0;
	if (ys)
		ys[0] = y;

	/*
	 * y accumulates in Q24 even though everything it is built from is Q16.
	 * Rounding the state to Q16 at every step costs about 1.5e-5 each time
	 * and it is a random walk, so sixty-four steps drifted four digits of a
	 * decay curve; carrying eight more fractional bits moves that error
	 * below the rounding of the answer.
	 */
	{
		int64_t y24 = (int64_t)y0 << 8;

		for (i = 0; i < steps; i++) {
			q16_t ya = (q16_t)(y24 >> 8);
			q16_t k1 = gexpr_eval2(f, x, ya);
			q16_t k2 = gexpr_eval2(f, x + half,
			                        gq_sat(ya + gq_mul(k1, half)));
			q16_t k3 = gexpr_eval2(f, x + half,
			                        gq_sat(ya + gq_mul(k2, half)));
			q16_t k4 = gexpr_eval2(f, x + h,
			                        gq_sat(ya + gq_mul(k3, h)));
			int64_t sum = (int64_t)k1 + 2 * (int64_t)k2 +
			              2 * (int64_t)k3 + (int64_t)k4;

			/* sum/6 is the Q16 slope; times h is Q32, kept as Q24. */
			y24 += (sum * (int64_t)h) / (6 * (int64_t)Q16_ONE) * 256;
			x = gq_sat(x + h);
			if (ys)
				ys[i + 1] = (q16_t)(y24 >> 8);
		}
		y = (q16_t)(y24 >> 8);
	}
	return GEXPR_DIFF_OK;
}

/* ---- printing ---- */

/*
 * Precedence of the text a node prints as, matching the parser's levels: + and
 * - are loosest, then * and /, then a leading minus, then ^, then a literal or
 * a function call. A child is bracketed when its own level is below the level
 * its position allows.
 */
enum { UP_ADD = 1, UP_MUL = 2, UP_NEG = 3, UP_POW = 4, UP_ATOM = 5 };

typedef struct {
	char *out;
	int cap;
	int len;
	int ok;
} up_t;

static void up_char(up_t *u, char c)
{
	if (!u->ok)
		return;
	/* One byte stays free for the terminator. */
	if (u->len + 1 >= u->cap) {
		u->ok = 0;
		return;
	}
	u->out[u->len++] = c;
}

static void up_text(up_t *u, const char *s)
{
	while (*s)
		up_char(u, *s++);
}

static void up_uint(up_t *u, int64_t v)
{
	char digits[12];
	int n = 0;

	do {
		digits[n++] = (char)('0' + (int)(v % 10));
		v /= 10;
	} while (v > 0);
	while (n > 0)
		up_char(u, digits[--n]);
}

static const int32_t up_p10[9] = { 1, 10, 100, 1000, 10000, 100000,
                                   1000000, 10000000, 100000000 };

/*
 * A number as a decimal the parser reads back to within one Q16 step: the
 * fewest decimals that round-trip, and eight when none does. Integers stay
 * integers, which is what keeps "3x^2+2" free of noise.
 */
static void up_num(up_t *u, int32_t v)
{
	int64_t a = v < 0 ? -(int64_t)v : v;
	int64_t scaled, ip, fr;
	int d, i;

	if (v < 0)
		up_char(u, '-');
	for (d = 0; d < 8; d++) {
		scaled = (a * up_p10[d] + 32768) >> 16;
		ip = scaled / up_p10[d];
		fr = scaled % up_p10[d];
		if (((ip << 16) + ((fr << 16) / up_p10[d])) == a)
			break;
	}
	scaled = (a * up_p10[d] + 32768) >> 16;
	ip = scaled / up_p10[d];
	fr = scaled % up_p10[d];
	up_uint(u, ip);
	if (d > 0) {
		up_char(u, '.');
		for (i = d - 1; i >= 0; i--)
			up_char(u, (char)('0' + (int)(fr / up_p10[i] % 10)));
	}
}

static int up_prec(const gexpr_t *e, int i)
{
	const gnode_t *n = &e->node[i];

	switch (n->op) {
	case G_ADD:
	case G_SUB:
		return UP_ADD;
	case G_MUL:
	case G_DIV:
		return UP_MUL;
	case G_NEG:
		return UP_NEG;
	case G_POW:
		return UP_POW;
	case G_NUM:
		return n->num < 0 ? UP_NEG : UP_ATOM;
	default:
		return UP_ATOM;
	}
}

/*
 * True when the text of node i begins with a letter, so it can follow a number
 * without a '*': "2x", "3sin(x)", "2x^2". A bracketed operand needs no '*'
 * either and is handled by the caller, since it is unambiguous.
 */
static int up_implicit(const gexpr_t *e, int i)
{
	int op = e->node[i].op;

	if (op == G_POW)
		return up_prec(e, e->node[i].a) < UP_ATOM ||
		       up_implicit(e, e->node[i].a);
	return op == G_X || op == G_SIN || op == G_COS || op == G_TAN ||
	       op == G_SQRT || op == G_ABS || op == G_LN || op == G_EXP;
}

static void up_node(up_t *u, const gexpr_t *e, int i, int min);

static void up_call(up_t *u, const gexpr_t *e, int i, const char *name)
{
	up_text(u, name);
	up_char(u, '(');
	up_node(u, e, e->node[i].a, UP_ADD);
	up_char(u, ')');
}

static void up_node(up_t *u, const gexpr_t *e, int i, int min)
{
	const gnode_t *n;
	int paren;

	if (!u->ok)
		return;
	if (i < 0) {
		u->ok = 0;
		return;
	}
	n = &e->node[i];
	paren = up_prec(e, i) < min;
	if (paren)
		up_char(u, '(');
	switch (n->op) {
	case G_NUM:
		up_num(u, n->num);
		break;
	case G_X:
	case G_Y:
		up_char(u, n->op == G_Y ? 'y' : 'x');
		break;
	case G_ADD:
	case G_SUB:
		up_node(u, e, n->a, UP_ADD);
		/* a + (-u) is a - u: it reads better and re-parses to the same
		 * value, and the same holds for a - (-u). */
		if (e->node[n->b].op == G_NEG) {
			up_char(u, n->op == G_ADD ? '-' : '+');
			up_node(u, e, e->node[n->b].a, UP_MUL);
		} else {
			up_char(u, n->op == G_ADD ? '+' : '-');
			up_node(u, e, n->b, UP_MUL);
		}
		break;
	case G_MUL:
		up_node(u, e, n->a, UP_MUL);
		if (up_prec(e, n->b) >= UP_POW && !up_implicit(e, n->b))
			up_char(u, '*');
		up_node(u, e, n->b, UP_POW);
		break;
	case G_DIV:
		up_node(u, e, n->a, UP_MUL);
		up_char(u, '/');
		up_node(u, e, n->b, UP_POW);
		break;
	case G_POW:
		up_node(u, e, n->a, UP_ATOM);
		up_char(u, '^');
		up_node(u, e, n->b, UP_NEG);
		break;
	case G_NEG:
		up_char(u, '-');
		up_node(u, e, n->a, UP_MUL);
		break;
	case G_SIN:
		up_call(u, e, i, "sin");
		break;
	case G_COS:
		up_call(u, e, i, "cos");
		break;
	case G_TAN:
		up_call(u, e, i, "tan");
		break;
	case G_SQRT:
		up_call(u, e, i, "sqrt");
		break;
	case G_ABS:
		up_call(u, e, i, "abs");
		break;
	case G_LN:
		up_call(u, e, i, "ln");
		break;
	case G_EXP:
		up_call(u, e, i, "exp");
		break;
	default:
		u->ok = 0;
	}
	if (paren)
		up_char(u, ')');
}

int gexpr_unparse(const gexpr_t *e, char *out, int outlen)
{
	up_t u;

	if (!e || !out || outlen <= 0)
		return -1;
	out[0] = 0;
	if (e->root < 0 || e->root >= e->n)
		return -1;
	u.out = out;
	u.cap = outlen;
	u.len = 0;
	u.ok = 1;
	up_node(&u, e, e->root, UP_ADD);
	if (!u.ok)
		return -1;
	out[u.len] = 0;
	return 0;
}

/* ---- definite integrals ---- */

/*
 * Simpson's rule over one panel: with width = b-a in Q16 units and s the
 * weighted sample sum in Q16 units, the value in Q16 units is
 * width*s/(6*65536). Both factors can be large, so the product is range
 * checked instead of wrapped, and a panel that leaves Q16 is a failure: a
 * saturated panel would make a divergent integral look answerable.
 */
static int simpson_panel(int64_t width, int64_t s, q16_t *out)
{
	const int64_t den = 6 * (int64_t)Q16_ONE;
	int64_t w = width < 0 ? -width : width;
	int64_t v = s < 0 ? -s : s;
	int64_t num, q;

	if (w != 0 && v > INT64_MAX / w)
		return -1;
	num = width * s;
	q = (num >= 0 ? num + den / 2 : num - den / 2) / den;
	if (q > Q16_MAX || q < Q16_MIN)
		return -1;
	*out = (q16_t)q;
	return 0;
}

/*
 * The work budget. Simpson converges quadratically on a smooth function and
 * barely at all on an endpoint singularity, so an unbounded loop is not an
 * option either way. 2000 samples is far more than any closed-form case in
 * test_expr uses - the worst is 117, over six periods of sin - while a
 * genuine singularity exhausts it and fails. The depth limit is the second
 * guard: a panel narrower than four Q16 steps cannot be halved onto
 * representable midpoints, so twenty levels reach that width from any Q16
 * interval.
 */
#define INT_MAX_EVALS 2000
#define INT_MAX_DEPTH 20

typedef struct {
	const gexpr_t *f;
	int evals;
} it_t;

/*
 * One sample. gexpr_eval reports "no value here" by saturating, which is the
 * only signal a singularity gives; an integral of a function that is not
 * finite across the interval cannot be bounded, so a saturated sample fails
 * the whole call rather than entering the sum as a clamp.
 */
static int isample(it_t *g, q16_t x, q16_t *out)
{
	q16_t v;

	if (g->evals >= INT_MAX_EVALS)
		return -1;
	g->evals++;
	v = gexpr_eval(g->f, x);
	if (v == Q16_MAX || v == Q16_MIN)
		return -1;
	*out = v;
	return 0;
}

/* The midpoint in Q16, exact enough that it always lies between a and b. */
static q16_t imid(q16_t a, q16_t b)
{
	return (q16_t)(((int64_t)a + b) / 2);
}

/*
 * Refines [a, b] until the two-panel Simpson sum agrees with the one-panel
 * estimate to `tol`. The split runs down the tree instead of in an explicit
 * stack, so the budget is one frame per active level; the panel value and both
 * sample sets travel by value.
 *
 * Failing to reach `tol` inside the depth or evaluation budget is reported as
 * failure. A number with an error estimate larger than what the answer needs
 * is exactly the kind of answer this engine refuses to give, and the caller
 * has no way to tell it from a good one.
 */
static int adapt(it_t *g, q16_t a, q16_t b, q16_t fa, q16_t fm, q16_t fb,
                 q16_t whole, int64_t tol, int depth, q16_t *out)
{
	q16_t m = imid(a, b);
	q16_t lm = imid(a, m);
	q16_t rm = imid(m, b);
	q16_t flm, frm, left, right, lv, rv;
	int64_t width = (int64_t)b - a;
	int64_t total, d, err;

	/*
	 * A panel under four Q16 steps has no pair of half-panels with
	 * representable midpoints: imid() would return an endpoint, the "half"
	 * panel would be empty and the other lopsided, and the two-panel test
	 * would compare against a rule that cannot converge. Its estimate is
	 * already the best this grid can carry, so it is accepted as it stands.
	 */
	if (width < 4) {
		*out = whole;
		return 0;
	}
	if (depth <= 0 || g->evals + 2 > INT_MAX_EVALS)
		return -1;
	if (isample(g, lm, &flm) < 0 || isample(g, rm, &frm) < 0)
		return -1;
	if (simpson_panel((int64_t)m - a,
	                  (int64_t)fa + 4 * (int64_t)flm + fm, &left) < 0 ||
	    simpson_panel((int64_t)b - m,
	                  (int64_t)fm + 4 * (int64_t)frm + fb, &right) < 0)
		return -1;
	total = (int64_t)left + right;
	if (total > Q16_MAX || total < Q16_MIN)
		return -1;
	d = total - whole;
	/*
	 * The two-panel and one-panel sums differ by about fifteen times the
	 * remaining error when the function is smooth, which is the usual
	 * |d|/15 test. This code uses |d| itself. That is deliberate: on a kink
	 * or near an endpoint where a derivative blows up, the fifteen-to-one
	 * factor is not a bound but an underestimate, and a panel accepted on it
	 * left sqrt(x) over [0,1] at 0.6665 instead of 0.6667. Demanding the two
	 * sums agree to the answer's own tolerance on every panel costs about a
	 * third more samples on the smooth cases and fixed every such case
	 * measured.
	 */
	err = d < 0 ? -d : d;
	if (err <= tol) {
		/* Richardson: the refined sum plus the error it is expected to
		 * carry. In Q16 the correction is often under one step, so it
		 * rounds to nothing rather than biasing the answer. */
		total += d / 15;
		if (total > Q16_MAX || total < Q16_MIN)
			return -1;
		*out = (q16_t)total;
		return 0;
	}
	/* Halving the allowance per side keeps the total error under tol. It
	 * stops halving at two Q16 steps: below that, integer noise in the
	 * samples makes the panel test unsatisfiable, which would refine a
	 * smooth function forever and then fail it. */
	if (tol > 2)
		tol /= 2;
	if (adapt(g, a, m, fa, flm, fm, left, tol, depth - 1, &lv) < 0 ||
	    adapt(g, m, b, fm, frm, fb, right, tol, depth - 1, &rv) < 0)
		return -1;
	total = (int64_t)lv + rv;
	if (total > Q16_MAX || total < Q16_MIN)
		return -1;
	*out = (q16_t)total;
	return 0;
}

int gexpr_integrate(const gexpr_t *f, q16_t a, q16_t b, q16_t *out)
{
	it_t g;
	q16_t fa, fb, fm, whole, total;
	int64_t width, tol, est;

	if (!f || !out || f->root < 0 || f->root >= f->n)
		return GEXPR_INT_UNBOUNDED;
	if (a == b) {
		*out = 0;
		return GEXPR_INT_OK;
	}
	/* Out of order is the negated integral; integrating the other way round
	 * also keeps every panel's width non-negative. */
	if (b < a) {
		q16_t v;
		int r = gexpr_integrate(f, b, a, &v);

		if (r != GEXPR_INT_OK || v == Q16_MIN)
			return GEXPR_INT_UNBOUNDED;
		*out = gq_sat(-(int64_t)v);
		return GEXPR_INT_OK;
	}
	g.f = f;
	g.evals = 0;
	width = (int64_t)b - a;
	fm = 0;
	if (isample(&g, a, &fa) < 0 || isample(&g, b, &fb) < 0 ||
	    isample(&g, imid(a, b), &fm) < 0)
		return GEXPR_INT_UNBOUNDED;
	if (simpson_panel(width, (int64_t)fa + 4 * (int64_t)fm + fb,
	                  &whole) < 0)
		return GEXPR_INT_UNBOUNDED;
	/*
	 * The tolerance has two parts, both measured from things the engine knows
	 * before it refines. The first is relative to the estimate: 1/16384 is
	 * about an eighth of the last of the four significant digits
	 * gexpr_format prints, so a panel that passes cannot move that digit on
	 * its own. The second is the absolute floor a fixed-point sample imposes:
	 * one Q16 step of error in f across this width moves the integral by
	 * width/65536 Q16 units, and 1/8192 allows eight such steps for the
	 * functions whose own series are a few steps off.
	 */
	est = whole < 0 ? -(int64_t)whole : whole;
	tol = est / 16384 + width / 8192 + 4;
	if (adapt(&g, a, b, fa, fm, fb, whole, tol, INT_MAX_DEPTH, &total) < 0)
		return GEXPR_INT_UNBOUNDED;
	*out = total;
	return GEXPR_INT_OK;
}

/* ---- typed derivative answers ---- */

#define EXPR_TEXT_MAX 256

int gexpr_derivative_answer(const char *text, char *out, int outlen)
{
	static const char *const forms[] = { "d/dx", "diff", "derivative" };
	char inner[EXPR_TEXT_MAX];
	char err[48];
	gexpr_t f, d;
	const char *p;
	size_t n, room;
	int k;

	if (!text || !out || outlen <= 0)
		return -1;
	p = text;
	while (*p == ' ' || *p == '\t')
		p++;
	n = strlen(text);
	while (n > 0 && (text[n - 1] == ' ' || text[n - 1] == '\t'))
		n--;
	if (n == 0 || (size_t)(p - text) >= n)
		return -1;
	room = (size_t)(text + n - p);
	for (k = 0; k < (int)(sizeof forms / sizeof forms[0]); k++) {
		size_t flen = strlen(forms[k]);
		size_t inner_len;
		const char *open, *start;

		/* Keyword, then a bracket, then at least one character and the
		 * closing bracket. The closing bracket has to be the last
		 * character, so "d/dx(x^2)+1" is not an answer about x^2. */
		if (room < flen + 2 || text[n - 1] != ')')
			continue;
		if (strncmp(p, forms[k], flen) != 0)
			continue;
		open = p + flen;
		while (*open == ' ' || *open == '\t')
			open++;
		if (*open != '(')
			continue;
		start = open + 1;
		inner_len = (size_t)(text + n - 1 - start);
		if (inner_len == 0 || inner_len >= sizeof inner)
			continue;
		memcpy(inner, start, inner_len);
		inner[inner_len] = 0;
		if (gexpr_parse(inner, &f, err, sizeof err) < 0)
			continue;
		if (gexpr_derive(&f, &d) != GEXPR_DIFF_OK)
			continue;
		return gexpr_unparse(&d, out, outlen);
	}
	return -1;
}

/* ---- typed integral answers ---- */

/*
 * Splits "f, a, b" at the commas that are not inside brackets. Returns 0 only
 * for exactly three non-empty parts with balanced brackets, so an extra comma,
 * a stray bracket or trailing text cannot be read as part of the bounds.
 */
static int split_three(const char *s, int n, const char **part, int *plen)
{
	int depth = 0, k = 0;

	part[0] = s;
	for (int i = 0; i < n; i++) {
		if (s[i] == '(') {
			depth++;
		} else if (s[i] == ')') {
			if (--depth < 0)
				return -1;
		} else if (s[i] == ',' && depth == 0) {
			if (k == 2)
				return -1;
			plen[k] = (int)(s + i - part[k]);
			part[++k] = s + i + 1;
		}
	}
	if (depth != 0 || k != 2)
		return -1;
	plen[2] = (int)(s + n - part[2]);
	for (int i = 0; i < 3; i++)
		if (plen[i] <= 0)
			return -1;
	return 0;
}

/*
 * A bound is any constant expression gexpr_parse accepts: "0", "-1", "0.5",
 * "pi", "2*pi", "1/2", "sqrt(2)". It is parsed like the integrand and then
 * evaluated once at x = 0, which is how the typed path already decides that an
 * expression is a number.
 *
 * That quantises the bound to Q16 before the integral runs, so pi is 3.14159
 * and 2*pi is 6.28317 rather than the real constants. The answer is the
 * integral over the quantised interval, which is why integrate(x,0,pi) prints
 * 4.935 (4.93478...) rather than 4.9348: the difference is the bound, not the
 * quadrature.
 *
 * A bound that depends on x has no single value as a limit, and one that fails
 * to parse or has no finite Q16 value refuses the whole call rather than
 * quietly integrating over something else.
 */
static int parse_bound(const char *s, int n, q16_t *out)
{
	char text[EXPR_TEXT_MAX];
	char err[48];
	gexpr_t e;
	q16_t v;
	int i = 0;

	while (i < n && (s[i] == ' ' || s[i] == '\t'))
		i++;
	while (n > i && (s[n - 1] == ' ' || s[n - 1] == '\t'))
		n--;
	if (n <= i || n - i >= (int)sizeof text)
		return -1;
	memcpy(text, s + i, (size_t)(n - i));
	text[n - i] = 0;
	if (gexpr_parse(text, &e, err, sizeof err) < 0 || gexpr_has_x(&e))
		return -1;
	v = gexpr_eval(&e, 0);
	if (v == Q16_MAX || v == Q16_MIN)
		return -1;
	*out = v;
	return 0;
}

int gexpr_integral_answer(const char *text, char *out, int outlen)
{
	static const char *const forms[] = { "integrate", "int" };
	char inner[EXPR_TEXT_MAX];
	char expr_text[EXPR_TEXT_MAX];
	char err[48];
	const char *part[3];
	int plen[3];
	gexpr_t f;
	const char *p;
	size_t n, room;
	q16_t a, b, v;
	int k;

	if (!text || !out || outlen <= 0)
		return -1;
	p = text;
	while (*p == ' ' || *p == '\t')
		p++;
	n = strlen(text);
	while (n > 0 && (text[n - 1] == ' ' || text[n - 1] == '\t'))
		n--;
	if (n == 0 || (size_t)(p - text) >= n)
		return -1;
	room = (size_t)(text + n - p);
	for (k = 0; k < (int)(sizeof forms / sizeof forms[0]); k++) {
		size_t flen = strlen(forms[k]);
		size_t inner_len;
		const char *open;
		int i;

		/* Keyword, then a bracket, then three parts whose brackets balance
		 * and a closing bracket as the last character, so
		 * "integrate(x^2,0,3)+1" is not an answer about x^2. */
		if (room < flen + 2 || text[n - 1] != ')')
			continue;
		if (strncmp(p, forms[k], flen) != 0)
			continue;
		open = p + flen;
		while (*open == ' ' || *open == '\t')
			open++;
		if (*open != '(')
			continue;
		inner_len = (size_t)(text + n - 1 - (open + 1));
		if (inner_len == 0 || inner_len >= sizeof inner)
			continue;
		memcpy(inner, open + 1, inner_len);
		inner[inner_len] = 0;
		if (split_three(inner, (int)inner_len, part, plen) < 0) {
			/*
			 * One expression and no bounds: the indefinite form. The
			 * answer is the antiderivative as an expression, which is
			 * what "+ C" stands for.
			 */
			gexpr_t anti;
			char sym[EXPR_TEXT_MAX];
			size_t slen;

			if (gexpr_parse(inner, &f, err, sizeof err) < 0)
				continue;
			if (gexpr_integrate_symbolic(&f, &anti) != GEXPR_DIFF_OK)
				continue;
			if (gexpr_unparse(&anti, sym, (int)sizeof sym) < 0)
				continue;
			slen = strlen(sym);
			if (slen + 5 > (size_t)outlen)
				continue;
			memcpy(out, sym, slen);
			memcpy(out + slen, " + C", 5);
			return 0;
		}
		if (parse_bound(part[1], plen[1], &a) < 0 ||
		    parse_bound(part[2], plen[2], &b) < 0)
			continue;
		for (i = 0; i < plen[0]; i++)
			expr_text[i] = part[0][i];
		expr_text[i] = 0;
		if (gexpr_parse(expr_text, &f, err, sizeof err) < 0)
			continue;
		if (gexpr_integrate(&f, a, b, &v) != GEXPR_INT_OK)
			continue;
		return gexpr_format(v, out, outlen);
	}
	return -1;
}

/* ---- series(expr, a, n) as a typed answer ---- */

int gexpr_series_answer(const char *text, char *out, int outlen)
{
	static const char *const forms[] = { "series", "taylor" };
	char inner[EXPR_TEXT_MAX];
	char expr_text[EXPR_TEXT_MAX];
	char err[48];
	const char *part[3];
	int plen[3];
	gexpr_t f, poly;
	const char *p;
	size_t n, room;
	q16_t a, cnt;
	int k;

	if (!text || !out || outlen <= 0)
		return -1;
	p = text;
	while (*p == ' ' || *p == '\t')
		p++;
	n = strlen(text);
	while (n > 0 && (text[n - 1] == ' ' || text[n - 1] == '\t'))
		n--;
	if (n == 0 || (size_t)(p - text) >= n)
		return -1;
	room = (size_t)(text + n - p);

	for (k = 0; k < (int)(sizeof forms / sizeof forms[0]); k++) {
		size_t flen = strlen(forms[k]);
		size_t inner_len;
		const char *open;
		int i, terms;

		if (room < flen + 2 || text[n - 1] != ')')
			continue;
		if (strncmp(p, forms[k], flen) != 0)
			continue;
		open = p + flen;
		while (*open == ' ' || *open == '\t')
			open++;
		if (*open != '(')
			continue;
		inner_len = (size_t)(text + n - 1 - (open + 1));
		if (inner_len == 0 || inner_len >= sizeof inner)
			continue;
		memcpy(inner, open + 1, inner_len);
		inner[inner_len] = 0;
		if (split_three(inner, (int)inner_len, part, plen) < 0)
			continue;
		if (parse_bound(part[1], plen[1], &a) < 0 ||
		    parse_bound(part[2], plen[2], &cnt) < 0)
			continue;
		terms = (int)(cnt >> 16);
		if (terms < 1 || (cnt & 0xFFFF) != 0)
			continue;
		for (i = 0; i < plen[0]; i++)
			expr_text[i] = part[0][i];
		expr_text[i] = 0;
		if (gexpr_parse(expr_text, &f, err, sizeof err) < 0)
			continue;
		if (gexpr_taylor(&f, a, terms, &poly) != GEXPR_DIFF_OK)
			continue;
		if (gexpr_unparse(&poly, out, outlen) < 0)
			continue;
		return 0;
	}
	return -1;
}

/* ---- ode(expr, x0, y0, xn, n) as a typed answer ---- */

/*
 * Splits "ode(f,x0,y0,xn,n)": four constants and an expression that may
 * mention both x and y. Kept next to the other forms rather than in a shared
 * helper because each form takes a different number of parts and a different
 * mix of expression and number.
 */
static int split_n(const char *s, int n, int count, const char **part, int *plen)
{
	int depth = 0;
	int field = 0;
	int i;

	plen[0] = 0;
	for (i = 0; i < n; i++) {
		char c = s[i];

		if (c == '(') {
			depth++;
		} else if (c == ')') {
			if (--depth < 0)
				return -1;
		} else if (c == ',' && depth == 0) {
			if (++field >= count)
				return -1;
			part[field] = s + i + 1;
			plen[field] = 0;
			continue;
		}
		if (field < count) {
			if (plen[field] == 0)
				part[field] = s + i;
			plen[field]++;
		}
	}
	if (depth != 0 || field != count - 1)
		return -1;
	for (i = 0; i < count; i++)
		if (plen[i] == 0)
			return -1;
	return 0;
}

int gexpr_ode_answer(const char *text, char *out, int outlen)
{
	char inner[EXPR_TEXT_MAX];
	char expr_text[EXPR_TEXT_MAX];
	char err[48];
	const char *part[5];
	int plen[5];
	gexpr_t f;
	const char *p;
	size_t n, room;
	q16_t x0, y0, xn, cnt, y;
	int i, steps, flen = 3;

	if (!text || !out || outlen <= 0)
		return -1;
	p = text;
	while (*p == ' ' || *p == '\t')
		p++;
	n = strlen(text);
	while (n > 0 && (text[n - 1] == ' ' || text[n - 1] == '\t'))
		n--;
	if (n == 0 || (size_t)(p - text) >= n)
		return -1;
	room = (size_t)(text + n - p);

	if (room < (size_t)flen + 2 || text[n - 1] != ')')
		return -1;
	if (strncmp(p, "ode", flen) != 0)
		return -1;
	p += flen;
	while (*p == ' ' || *p == '\t')
		p++;
	if (*p != '(')
		return -1;
	p++;

	{
		size_t inner_len = (size_t)(text + n - 1 - p);

		if (inner_len == 0 || inner_len >= sizeof inner)
			return -1;
		memcpy(inner, p, inner_len);
		inner[inner_len] = 0;
	}
	if (split_n(inner, (int)strlen(inner), 5, part, plen) < 0)
		return -1;
	if (parse_bound(part[1], plen[1], &x0) < 0 ||
	    parse_bound(part[2], plen[2], &y0) < 0 ||
	    parse_bound(part[3], plen[3], &xn) < 0 ||
	    parse_bound(part[4], plen[4], &cnt) < 0)
		return -1;
	if ((cnt & 0xFFFF) != 0)
		return -1;
	steps = (int)(cnt >> 16);
	for (i = 0; i < plen[0] && i < (int)sizeof expr_text - 1; i++)
		expr_text[i] = part[0][i];
	expr_text[i] = 0;
	if (gexpr_parse(expr_text, &f, err, sizeof err) < 0)
		return -1;
	/*
	 * y' = f(x) alone is legitimate - a pure integral with a step - so there
	 * is no requirement that y appears.
	 */
	{
		q16_t ys[EXPR_MAX_ODE_STEPS + 1];

		if (gexpr_rk4(&f, x0, y0, xn, steps, ys,
		              (int)(sizeof ys / sizeof ys[0])) != GEXPR_DIFF_OK)
			return -1;
		y = ys[steps];
	}
	return gexpr_format(y, out, outlen);
}
