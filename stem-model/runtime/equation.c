/*
 * One equation solver: reduce "lhs = rhs" to a polynomial when it is one,
 * solve degrees 0-2 exactly, and answer two shapes that are not polynomials
 * with one closed form each.
 *
 * The degree-1 and degree-2 arithmetic is the arithmetic quad_solve already
 * used, moved here so there is one copy: the same rational discriminant, the
 * same integer isqrt for exact roots, the same sqrt12 scaling for irrational
 * ones, and the same domain guards. quad_solve is now a renderer over this.
 */
#include <stdint.h>

#include "equation.h"

#define EQ_Q16 65536LL

static const rat EQ_ZERO = { 0, 1 };
static const rat EQ_ONE = { 1, 1 };
static const rat EQ_TWO = { 2, 1 };
static const rat EQ_FOUR = { 4, 1 };

int eq_rat_cmp(const rat *a, const rat *b)
{
	long long l = a->n * b->d, r = b->n * a->d;

	return (l > r) - (l < r);
}

/* a*b inside the domain dec4_render accepts; -1 rather than a wrapped value. */
static int mul_ll(long long a, long long b, long long *out)
{
	if (a == 0 || b == 0) {
		*out = 0;
		return 0;
	}
	if (a > 0) {
		if (b > 0) {
			if (a > WIDE_LIMIT / b)
				return -1;
		} else if (b < -WIDE_LIMIT / a) {
			return -1;
		}
	} else {
		if (b > 0) {
			if (a < -WIDE_LIMIT / b)
				return -1;
		} else if (b < WIDE_LIMIT / a) {
			return -1;
		}
	}
	*out = a * b;
	return 0;
}

/* ---- polynomial reduction ---- */

static void poly_zero(eq_poly_t *p)
{
	int i;

	for (i = 0; i <= EQ_MAX_DEGREE; i++) {
		p->c[i].n = 0;
		p->c[i].d = 1;
	}
	p->degree = -1;
}

static void poly_trim(eq_poly_t *p)
{
	int i;

	for (i = EQ_MAX_DEGREE; i >= 0; i--) {
		if (p->c[i].n != 0) {
			p->degree = i;
			return;
		}
	}
	p->degree = -1;
}

static int poly_add(eq_poly_t *r, const eq_poly_t *a, const eq_poly_t *b,
                    int negate_b)
{
	eq_poly_t s;
	int i, top = a->degree > b->degree ? a->degree : b->degree;

	poly_zero(&s);
	for (i = 0; i <= top; i++) {
		rat x = EQ_ZERO, y = EQ_ZERO;

		if (i <= a->degree)
			x = a->c[i];
		if (i <= b->degree) {
			y = b->c[i];
			if (negate_b)
				y.n = -y.n;
		}
		if (rat_add(&s.c[i], &x, &y) != 0)
			return -1;
	}
	poly_trim(&s);
	*r = s;
	return 0;
}

static int poly_mul(eq_poly_t *r, const eq_poly_t *a, const eq_poly_t *b)
{
	eq_poly_t s;
	rat t;
	int i, j;

	if (a->degree < 0 || b->degree < 0) {
		poly_zero(r);
		return 0;
	}
	if (a->degree + b->degree > EQ_MAX_DEGREE)
		return 1;
	poly_zero(&s);
	for (i = 0; i <= a->degree; i++) {
		for (j = 0; j <= b->degree; j++) {
			if (rat_mul(&t, &a->c[i], &b->c[j]) != 0)
				return -1;
			if (rat_add(&s.c[i + j], &s.c[i + j], &t) != 0)
				return -1;
		}
	}
	poly_trim(&s);
	*r = s;
	return 0;
}

/*
 * 0: p is the polynomial. 1: not a polynomial in the engine's domain, which
 * includes the degree and exponent caps. -1: an exact intermediate left the
 * fixed-point domain.
 */
static int poly_node(const gexpr_t *e, int i, eq_poly_t *p)
{
	eq_poly_t a, b;
	const gnode_t *n;
	int rc, k;

	poly_zero(p);
	if (i < 0 || i >= e->n)
		return 1;
	n = &e->node[i];
	switch (n->op) {
	case G_NUM:
		if (n->num != 0) {
			p->c[0].n = n->num;
			p->c[0].d = EQ_Q16;
			if (rat_norm(&p->c[0]) != 0)
				return -1;
			p->degree = 0;
		}
		return 0;
	case G_X:
		p->c[1].n = 1;
		p->degree = 1;
		return 0;
	case G_ADD:
	case G_SUB:
		if ((rc = poly_node(e, n->a, &a)) != 0)
			return rc;
		if ((rc = poly_node(e, n->b, &b)) != 0)
			return rc;
		return poly_add(p, &a, &b, n->op == G_SUB);
	case G_NEG:
		if ((rc = poly_node(e, n->a, &a)) != 0)
			return rc;
		*p = a;
		for (k = 0; k <= p->degree; k++)
			p->c[k].n = -p->c[k].n;
		return 0;
	case G_MUL:
		if ((rc = poly_node(e, n->a, &a)) != 0)
			return rc;
		if ((rc = poly_node(e, n->b, &b)) != 0)
			return rc;
		return poly_mul(p, &a, &b);
	case G_DIV:
		if ((rc = poly_node(e, n->a, &a)) != 0)
			return rc;
		if ((rc = poly_node(e, n->b, &b)) != 0)
			return rc;
		if (b.degree > 0 || b.c[0].n == 0)
			return 1;	/* only division by a constant is polynomial */
		p->degree = a.degree;
		for (k = 0; k <= a.degree; k++)
			if (rat_div(&p->c[k], &a.c[k], &b.c[0]) != 0)
				return -1;
		return 0;
	case G_POW:
		if (n->b < 0 || n->b >= e->n)
			return 1;
		if (e->node[n->b].op != G_NUM ||
		    (e->node[n->b].num & 0xFFFF) != 0)
			return 1;
		k = e->node[n->b].num >> 16;
		if (k < 0 || k > EQ_MAX_POWER)
			return 1;
		if ((rc = poly_node(e, n->a, &a)) != 0)
			return rc;
		p->c[0].n = 1;
		p->degree = 0;
		while (k-- > 0) {
			if ((rc = poly_mul(&b, p, &a)) != 0)
				return rc;
			*p = b;
		}
		return 0;
	default:
		return 1;
	}
}

void equation_poly_from(eq_poly_t *p, const rat *coef, int n)
{
	int i;

	poly_zero(p);
	if (n > EQ_MAX_DEGREE + 1)
		n = EQ_MAX_DEGREE + 1;
	for (i = 0; i < n; i++)
		p->c[i] = coef[i];
	poly_trim(p);
}

int equation_poly(const gexpr_t *lhs, const gexpr_t *rhs, eq_poly_t *p)
{
	eq_poly_t a, b;
	int rc;

	if ((rc = poly_node(lhs, lhs->root, &a)) != 0)
		return rc;
	if ((rc = poly_node(rhs, rhs->root, &b)) != 0)
		return rc;
	return poly_add(p, &a, &b, 1);
}

/* ---- degree 1 and degree 2 ---- */

static eq_status_t solve_linear(const eq_poly_t *p, eq_result_t *out)
{
	if (rat_div(&out->root[0], &p->c[0], &p->c[1]) != 0)
		return EQ_OVERFLOW;
	out->root[0].n = -out->root[0].n;
	out->n = 1;
	return EQ_SOLVED;
}

static eq_status_t solve_quadratic(const eq_poly_t *p, eq_result_t *out)
{
	const rat *a = &p->c[2], *b = &p->c[1], *c = &p->c[0];
	rat disc, s, t, t2;
	long long q, rt;

	if (rat_mul(&disc, b, b) != 0 ||
	    rat_mul(&t, a, c) != 0 ||
	    rat_mul(&t, &t, &EQ_FOUR) != 0 ||
	    rat_sub(&disc, &disc, &t) != 0)
		return EQ_OVERFLOW;
	if (disc.n < 0)
		return EQ_NONE;
	if (disc.n == 0) {
		if (rat_add(&t, a, a) != 0 ||
		    rat_div(&out->root[0], b, &t) != 0)
			return EQ_OVERFLOW;
		out->root[0].n = -out->root[0].n;
		out->n = 1;
		return EQ_SOLVED;
	}
	if (disc.n > WIDE_LIMIT / disc.d)
		return EQ_OVERFLOW;
	q = disc.n * disc.d;
	rt = (long long)isqrt64((unsigned long long)q);
	if (rt * rt == q) {
		s.n = rt;
		s.d = disc.d;
		if (rat_norm(&s) != 0)
			return EQ_OVERFLOW;
		if (rat_add(&t, b, &s) != 0 ||
		    rat_mul(&t2, a, &EQ_TWO) != 0 ||
		    rat_div(&out->root[0], &t, &t2) != 0 ||
		    rat_sub(&t, &s, b) != 0 ||
		    rat_div(&out->root[1], &t, &t2) != 0)
			return EQ_OVERFLOW;
		out->root[0].n = -out->root[0].n;
		if (eq_rat_cmp(&out->root[0], &out->root[1]) > 0) {
			rat sw = out->root[0];

			out->root[0] = out->root[1];
			out->root[1] = sw;
		}
		out->n = 2;
		return EQ_SOLVED;
	}
	{
		long long s12, eb, ea, bn_scaled, n1, n2, den;
		int rc = 0;

		s12 = sqrt12(disc.n, disc.d, &rc);
		if (rc != 0)
			return EQ_OVERFLOW;
		eb = pow10_index(b->d);
		ea = pow10_index(a->d);
		if (eb < 0 || ea < 0 || 12 - eb < 0 || 12 - ea < 0)
			return EQ_OVERFLOW;
		if (b->n > 900000LL || -b->n > 900000LL ||
		    a->n > 900000LL || -a->n > 900000LL)
			return EQ_OVERFLOW;
		bn_scaled = b->n * P10[12 - eb];
		if (bn_scaled > WIDE_LIMIT / 2 || -bn_scaled > WIDE_LIMIT / 2)
			return EQ_OVERFLOW;
		den = 2 * a->n * P10[12 - ea];
		n1 = -bn_scaled - s12;
		n2 = s12 - bn_scaled;
		if (den < 0) {
			den = -den;
			n1 = -n1;
			n2 = -n2;
		}
		if (n1 > n2) {
			long long sw = n1;

			n1 = n2;
			n2 = sw;
		}
		out->root[0].n = n1;
		out->root[0].d = den;
		out->root[1].n = n2;
		out->root[1].d = den;
		out->n = 2;
		out->approx = 1;
		return EQ_SOLVED;
	}
}

eq_status_t equation_solve_poly(const eq_poly_t *p, eq_result_t *out)
{
	out->n = 0;
	out->approx = 0;
	out->status = EQ_UNSUPPORTED;
	switch (p->degree) {
	case -1:
		out->status = EQ_ALL;
		break;
	case 0:
		out->status = EQ_NONE;
		break;
	case 1:
		out->status = solve_linear(p, out);
		break;
	case 2:
		out->status = solve_quadratic(p, out);
		break;
	default:
		break;
	}
	return out->status;
}

/* ---- the shapes that are not polynomials ---- */

/* Reduce a subtree to a constant; 0 on success. */
static int const_value(const gexpr_t *e, int i, rat *v)
{
	eq_poly_t p;
	int rc = poly_node(e, i, &p);

	if (rc != 0)
		return rc;
	if (p.degree > 0)
		return 1;
	*v = p.degree < 0 ? EQ_ZERO : p.c[0];
	return 0;
}

/* Reduce a subtree to m*x + c; 0 on success. */
static int linear_value(const gexpr_t *e, int i, rat *m, rat *c)
{
	eq_poly_t p;
	int rc = poly_node(e, i, &p);

	if (rc != 0)
		return rc;
	if (p.degree > 1)
		return 1;
	*m = p.degree >= 1 ? p.c[1] : EQ_ZERO;
	*c = p.degree >= 0 ? p.c[0] : EQ_ZERO;
	return 0;
}

/*
 * Find a * b^(m*x + c) on the left, with b a positive constant other than 1.
 * Returns the exponent's m and c. The product of a constant and a power is the
 * only shape accepted; a deeper product stays unsupported rather than guessed.
 */
static int match_exp(const gexpr_t *e, rat *a, rat *b, rat *m, rat *c)
{
	const gnode_t *n;
	int pi;

	if (e->root < 0)
		return 1;
	n = &e->node[e->root];
	*a = EQ_ONE;
	if (n->op == G_POW) {
		pi = e->root;
	} else if (n->op == G_MUL) {
		if (e->node[n->a].op == G_POW) {
			pi = n->a;
			if (const_value(e, n->b, a) != 0)
				return 1;
		} else if (e->node[n->b].op == G_POW) {
			pi = n->b;
			if (const_value(e, n->a, a) != 0)
				return 1;
		} else {
			return 1;
		}
	} else {
		return 1;
	}
	if (const_value(e, e->node[pi].a, b) != 0)
		return 1;
	if (b->n <= 0 || eq_rat_cmp(b, &EQ_ONE) == 0)
		return 1;
	if (linear_value(e, e->node[pi].b, m, c) != 0 || m->n == 0)
		return 1;
	return 0;
}

/* a*b^(m*x + c) = k  ->  x = (log_b(k/a) - c) / m. */
static eq_status_t solve_exp(const gexpr_t *lhs, const gexpr_t *rhs,
                             eq_result_t *out)
{
	rat a, b, m, c, k, ka;
	long long lk, lb, num, den, p1, p2;
	int rc = 0;

	if (const_value(rhs, rhs->root, &k) != 0)
		return EQ_UNSUPPORTED;
	if (match_exp(lhs, &a, &b, &m, &c) != 0 || a.n == 0)
		return EQ_UNSUPPORTED;
	if (rat_div(&ka, &k, &a) != 0)
		return EQ_OVERFLOW;
	/* A positive base raised to any power is positive, so the sign is a's. */
	if (ka.n <= 0)
		return EQ_NONE;
	lk = ln_9(ka.n, ka.d, &rc);
	if (rc != 0)
		return EQ_OVERFLOW;
	lb = ln_9(b.n, b.d, &rc);
	if (rc != 0)
		return EQ_OVERFLOW;
	/* x = (lk/lb - c)/m, kept as one rational so dec4_render rounds once. */
	if (mul_ll(lk, c.d, &p1) != 0 || mul_ll(c.n, lb, &p2) != 0)
		return EQ_OVERFLOW;
	num = p1 - p2;
	if (mul_ll(lb, c.d, &den) != 0 || mul_ll(num, m.d, &num) != 0 ||
	    mul_ll(den, m.n, &den) != 0)
		return EQ_OVERFLOW;
	if (den < 0) {
		den = -den;
		num = -num;
	}
	out->root[0].n = num;
	out->root[0].d = den;
	out->n = 1;
	out->approx = 1;
	return EQ_SOLVED;
}

/* Find a * x^p on the left with p an integer in [1, EQ_MAX_POWER]. */
static int match_power(const gexpr_t *e, rat *a, int *p)
{
	const gnode_t *n;
	int xi;

	if (e->root < 0)
		return 1;
	n = &e->node[e->root];
	*a = EQ_ONE;
	if (n->op == G_POW) {
		xi = e->root;
	} else if (n->op == G_MUL) {
		if (e->node[n->a].op == G_POW) {
			xi = n->a;
			if (const_value(e, n->b, a) != 0)
				return 1;
		} else if (e->node[n->b].op == G_POW) {
			xi = n->b;
			if (const_value(e, n->a, a) != 0)
				return 1;
		} else {
			return 1;
		}
	} else {
		return 1;
	}
	if (e->node[xi].a < 0 || e->node[xi].a >= e->n ||
	    e->node[e->node[xi].a].op != G_X)
		return 1;
	if (e->node[xi].b < 0 || e->node[xi].b >= e->n ||
	    e->node[e->node[xi].b].op != G_NUM ||
	    (e->node[e->node[xi].b].num & 0xFFFF) != 0)
		return 1;
	*p = e->node[e->node[xi].b].num >> 16;
	return (*p >= 1 && *p <= EQ_MAX_POWER) ? 0 : 1;
}

/* a*x^p = k  ->  x = (k/a)^(1/p), one root for odd p and a sign pair for even. */
static eq_status_t solve_power(const gexpr_t *lhs, const gexpr_t *rhs,
                               eq_result_t *out)
{
	rat a, t, k;
	long long l9, x9, r9;
	int p, neg = 0, rc = 0;

	if (const_value(rhs, rhs->root, &k) != 0)
		return EQ_UNSUPPORTED;
	if (match_power(lhs, &a, &p) != 0 || a.n == 0)
		return EQ_UNSUPPORTED;
	if (rat_div(&t, &k, &a) != 0)
		return EQ_OVERFLOW;
	if (t.n == 0) {
		out->root[0] = EQ_ZERO;
		out->n = 1;
		return EQ_SOLVED;
	}
	if (t.n < 0) {
		if ((p & 1) == 0)
			return EQ_NONE;
		t.n = -t.n;
		neg = 1;
	}
	if (p == 1) {
		out->root[0] = t;
		if (neg)
			out->root[0].n = -t.n;
		out->n = 1;
		return EQ_SOLVED;
	}
	l9 = ln_9(t.n, t.d, &rc);
	if (rc != 0)
		return EQ_OVERFLOW;
	x9 = l9 / p;
	r9 = exp_9(x9, &rc);
	if (rc != 0 || r9 <= 0)
		return EQ_OVERFLOW;
	out->approx = 1;
	if ((p & 1) == 0) {
		out->root[0].n = -r9;
		out->root[0].d = 1000000000LL;
		out->root[1].n = r9;
		out->root[1].d = 1000000000LL;
		out->n = 2;
	} else {
		out->root[0].n = neg ? -r9 : r9;
		out->root[0].d = 1000000000LL;
		out->n = 1;
	}
	return EQ_SOLVED;
}

/* ---- the entry point ---- */

static eq_status_t solve_shapes(const gexpr_t *lhs, const gexpr_t *rhs,
                                eq_result_t *out)
{
	eq_status_t st = solve_exp(lhs, rhs, out);

	if (st != EQ_UNSUPPORTED)
		return st;
	return solve_power(lhs, rhs, out);
}

eq_status_t equation_solve(const gexpr_t *lhs, const gexpr_t *rhs,
                           eq_result_t *out)
{
	eq_poly_t p;
	eq_status_t st;
	int rc;

	out->n = 0;
	out->approx = 0;
	rc = equation_poly(lhs, rhs, &p);
	if (rc == 0 && p.degree <= 2)
		return equation_solve_poly(&p, out);
	st = solve_shapes(lhs, rhs, out);
	if (st == EQ_UNSUPPORTED)
		st = solve_shapes(rhs, lhs, out);
	if (st == EQ_UNSUPPORTED)
		st = rc == -1 ? EQ_OVERFLOW : EQ_UNSUPPORTED;
	out->status = st;
	return st;
}
