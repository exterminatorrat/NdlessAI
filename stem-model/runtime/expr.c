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
	return (c >= '0' && c <= '9') || c == '.' || c == 'x' || c == '(' ||
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

static q16_t eval_node(const gexpr_t *e, int i, q16_t x)
{
	const gnode_t *n = &e->node[i];

	switch (n->op) {
	case G_NUM:
		return n->num;
	case G_X:
		return x;
	case G_ADD:
		return gq_sat((int64_t)eval_node(e, n->a, x) + eval_node(e, n->b, x));
	case G_SUB:
		return gq_sat((int64_t)eval_node(e, n->a, x) - eval_node(e, n->b, x));
	case G_MUL:
		return gq_mul(eval_node(e, n->a, x), eval_node(e, n->b, x));
	case G_DIV:
		return gq_div(eval_node(e, n->a, x), eval_node(e, n->b, x));
	case G_POW:
		return qpow(eval_node(e, n->a, x), eval_node(e, n->b, x));
	case G_SIN:
		return gq_sin(eval_node(e, n->a, x));
	case G_COS:
		return gq_cos(eval_node(e, n->a, x));
	case G_TAN:
		return gq_tan(eval_node(e, n->a, x));
	case G_NEG:
		return gq_sat(-(int64_t)eval_node(e, n->a, x));
	case G_SQRT:
		return gq_sqrt(eval_node(e, n->a, x));
	case G_ABS: {
		q16_t v = eval_node(e, n->a, x);

		return v < 0 ? gq_sat(-(int64_t)v) : v;
	}
	case G_LN:
		return gq_ln(eval_node(e, n->a, x));
	case G_EXP:
		return gq_exp(eval_node(e, n->a, x));
	default:
		return 0;
	}
}

q16_t gexpr_eval(const gexpr_t *e, q16_t x)
{
	if (e->root < 0)
		return 0;
	return eval_node(e, e->root, x);
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
