#include <math.h>
#include <stdio.h>
#include <string.h>

#include "solvers.h"
#include "solvers_subject.h"

static const rat RAT_2 = { 2, 1 };
static const rat RAT_3 = { 3, 1 };
static const rat RAT_4 = { 4, 1 };

static int rat_cmp(const rat *a, const rat *b)
{
	long long l = a->n * b->d, r = b->n * a->d;

	return (l > r) - (l < r);
}

static int solve_quad(const char *const *args, char *out, size_t outlen)
{
	char f1[48], f2[48], buf[112];
	rat a, b, c, disc, s, t, t2, r1, r2;
	int rc, eb, ea, fa, fb, fc;
	long long q, rt, bn_scaled, s12, n1, n2, den;

	if ((rc = parse_rat(args[0], &a, &fa)) != 0 ||
	    (rc = parse_rat(args[1], &b, &fb)) != 0 ||
	    (rc = parse_rat(args[2], &c, &fc)) != 0)
		return rc;
	/*
	 * Python feeds float tokens into sp.solve, which returns Float roots
	 * rendered by dec(); integer tokens give exact Integer/Rational roots
	 * rendered by fmt().  A float token whose value is exactly zero drops
	 * out of the polynomial, so it does not switch the mode.
	 */
	fa = fa && a.n != 0;
	fb = fb && b.n != 0;
	fc = fc && c.n != 0;
	if (a.n == 0) {
		if (b.n == 0) {
			out_set(out, outlen, "");
			return 0;
		}
		if ((rc = rat_div(&r1, &c, &b)) != 0)
			return rc;
		r1.n = -r1.n;
		if (fb || (fc && c.n != 0))
			dec4_render(r1.n, r1.d, f1, sizeof(f1));
		else
			fmt_rat(&r1, f1, sizeof(f1));
		snprintf(buf, sizeof(buf), "x=%s", f1);
		out_set(out, outlen, buf);
		return 0;
	}
	if ((rc = rat_mul(&disc, &b, &b)) != 0 ||
	    (rc = rat_mul(&t, &a, &c)) != 0 ||
	    (rc = rat_mul(&t, &t, &RAT_4)) != 0 ||
	    (rc = rat_sub(&disc, &disc, &t)) != 0)
		return rc;
	if (disc.n < 0)
		return -4;
	if (disc.n == 0) {
		if ((rc = rat_add(&t, &a, &a)) != 0 ||
		    (rc = rat_div(&r1, &b, &t)) != 0)
			return rc;
		r1.n = -r1.n;
		if (fa || fb || (fc && c.n != 0))
			dec4_render(r1.n, r1.d, f1, sizeof(f1));
		else
			fmt_rat(&r1, f1, sizeof(f1));
		snprintf(buf, sizeof(buf), "x=%s", f1);
		out_set(out, outlen, buf);
		return 0;
	}
	if (disc.n > WIDE_LIMIT / disc.d)
		return -4;
	q = disc.n * disc.d;
	rt = (long long)isqrt64((unsigned long long)q);
	if (rt * rt == q) {
		s.n = rt;
		s.d = disc.d;
		if ((rc = rat_norm(&s)) != 0)
			return rc;
		if ((rc = rat_add(&t, &b, &s)) != 0 ||
		    (rc = rat_mul(&t2, &a, &RAT_2)) != 0 ||
		    (rc = rat_div(&r1, &t, &t2)) != 0 ||
		    (rc = rat_sub(&t, &s, &b)) != 0 ||
		    (rc = rat_div(&r2, &t, &t2)) != 0)
			return rc;
		r1.n = -r1.n;
		if (fa || fb || (fc && c.n != 0)) {
			dec4_render(r1.n, r1.d, f1, sizeof(f1));
			dec4_render(r2.n, r2.d, f2, sizeof(f2));
		} else {
			fmt_rat(&r1, f1, sizeof(f1));
			fmt_rat(&r2, f2, sizeof(f2));
		}
		if (rat_cmp(&r1, &r2) > 0)
			snprintf(buf, sizeof(buf), "x=%s, x=%s", f2, f1);
		else
			snprintf(buf, sizeof(buf), "x=%s, x=%s", f1, f2);
		out_set(out, outlen, buf);
		return 0;
	}
	s12 = sqrt12(disc.n, disc.d, &rc);
	if (rc != 0)
		return rc;
	eb = pow10_index(b.d);
	ea = pow10_index(a.d);
	if (eb < 0 || ea < 0 || 12 - eb < 0 || 12 - ea < 0)
		return -4;
	if (b.n > 900000LL || -b.n > 900000LL || a.n > 900000LL ||
	    -a.n > 900000LL)
		return -4;
	bn_scaled = b.n * P10[12 - eb];
	if (bn_scaled > WIDE_LIMIT / 2 || -bn_scaled > WIDE_LIMIT / 2)
		return -4;
	den = 2 * a.n * P10[12 - ea];
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
	if ((rc = dec4_render(n1, den, f1, sizeof(f1))) != 0)
		return rc;
	if ((rc = dec4_render(n2, den, f2, sizeof(f2))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "x=%s, x=%s", f1, f2);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_deriv(const char *const *args, char *out, size_t outlen)
{
	rat a, b, c, d, x, t, r;
	int rc;
	char f1[48];

	if ((rc = parse_rat(args[0], &a, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &b, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &c, NULL)) != 0 ||
	    (rc = parse_rat(args[3], &d, NULL)) != 0 ||
	    (rc = parse_rat(args[4], &x, NULL)) != 0)
		return rc;
	if ((rc = rat_mul(&t, &a, &x)) != 0 ||
	    (rc = rat_mul(&t, &t, &x)) != 0 ||
	    (rc = rat_mul(&t, &t, &RAT_3)) != 0 ||
	    (rc = rat_mul(&r, &b, &x)) != 0 ||
	    (rc = rat_mul(&r, &r, &RAT_2)) != 0 ||
	    (rc = rat_add(&r, &r, &c)) != 0 ||
	    (rc = rat_add(&r, &r, &t)) != 0)
		return rc;
	(void)d;
	fmt_rat(&r, f1, sizeof(f1));
	out_set(out, outlen, f1);
	return 0;
}

static int solve_integr(const char *const *args, char *out, size_t outlen)
{
	rat a, b, c, d, lo, hi, h2, h3, h4, l2, l3, l4, t, r;
	int rc;
	char f1[48];

	if ((rc = parse_rat(args[0], &a, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &b, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &c, NULL)) != 0 ||
	    (rc = parse_rat(args[3], &d, NULL)) != 0 ||
	    (rc = parse_rat(args[4], &lo, NULL)) != 0 ||
	    (rc = parse_rat(args[5], &hi, NULL)) != 0)
		return rc;
	if ((rc = rat_mul(&h2, &hi, &hi)) != 0 ||
	    (rc = rat_mul(&h3, &h2, &hi)) != 0 ||
	    (rc = rat_mul(&h4, &h2, &h2)) != 0 ||
	    (rc = rat_mul(&l2, &lo, &lo)) != 0 ||
	    (rc = rat_mul(&l3, &l2, &lo)) != 0 ||
	    (rc = rat_mul(&l4, &l2, &l2)) != 0)
		return rc;
	if ((rc = rat_sub(&t, &h4, &l4)) != 0 ||
	    (rc = rat_mul(&t, &t, &a)) != 0 ||
	    (rc = rat_div(&t, &t, &RAT_4)) != 0)
		return rc;
	r = t;
	r = t;
	if ((rc = rat_sub(&t, &h3, &l3)) != 0 ||
	    (rc = rat_mul(&t, &t, &b)) != 0 ||
	    (rc = rat_div(&t, &t, &RAT_3)) != 0 ||
	    (rc = rat_add(&r, &r, &t)) != 0)
		return rc;
	if ((rc = rat_sub(&t, &h2, &l2)) != 0 ||
	    (rc = rat_mul(&t, &t, &c)) != 0 ||
	    (rc = rat_div(&t, &t, &RAT_2)) != 0 ||
	    (rc = rat_add(&r, &r, &t)) != 0)
		return rc;
	if ((rc = rat_sub(&t, &hi, &lo)) != 0 ||
	    (rc = rat_mul(&t, &t, &d)) != 0 ||
	    (rc = rat_add(&r, &r, &t)) != 0)
		return rc;
	fmt_rat(&r, f1, sizeof(f1));
	out_set(out, outlen, f1);
	return 0;
}

static int solve_linear2(const char *const *args, char *out, size_t outlen)
{
	rat a1, b1, c1, a2, b2, c2, det, t, x, y;
	int rc;
	char f1[48], f2[48], buf[104];

	if ((rc = parse_rat(args[0], &a1, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &b1, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &c1, NULL)) != 0 ||
	    (rc = parse_rat(args[3], &a2, NULL)) != 0 ||
	    (rc = parse_rat(args[4], &b2, NULL)) != 0 ||
	    (rc = parse_rat(args[5], &c2, NULL)) != 0)
		return rc;
	if ((rc = rat_mul(&t, &a1, &b2)) != 0 ||
	    (rc = rat_mul(&det, &a2, &b1)) != 0 ||
	    (rc = rat_sub(&det, &t, &det)) != 0)
		return rc;
	if (det.n == 0)
		return -4;
	if ((rc = rat_mul(&t, &c1, &b2)) != 0 ||
	    (rc = rat_mul(&x, &c2, &b1)) != 0 ||
	    (rc = rat_sub(&x, &t, &x)) != 0 ||
	    (rc = rat_div(&x, &x, &det)) != 0)
		return rc;
	if ((rc = rat_mul(&t, &a1, &c2)) != 0 ||
	    (rc = rat_mul(&y, &a2, &c1)) != 0 ||
	    (rc = rat_sub(&y, &t, &y)) != 0 ||
	    (rc = rat_div(&y, &y, &det)) != 0)
		return rc;
	fmt_rat(&x, f1, sizeof(f1));
	fmt_rat(&y, f2, sizeof(f2));
	snprintf(buf, sizeof(buf), "x=%s, y=%s", f1, f2);
	out_set(out, outlen, buf);
	return 0;
}

/*
 * noinline is load-bearing, not a hint: the fallback below is the one place in
 * the runtime that uses a double, and crosscheck.sh allows it only under this
 * function's name. Inlined into solvers_math_solve it would look like a
 * soft-float call in the per-token path.
 */
static int __attribute__((noinline))
solve_exp(const char *const *args, char *out, size_t outlen)
{
	rat base, k, p;
	int rc, m;
	char buf[48];

	if ((rc = parse_rat(args[0], &base, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &k, NULL)) != 0)
		return rc;
	if (base.n <= 0 || (base.n == base.d) || k.n <= 0)
		return -4;
	if (k.n == k.d) {
		out_set(out, outlen, "0");
		return 0;
	}
	p = base;
	for (m = 1; m <= 12; m++) {
		if (rat_cmp(&p, &k) == 0) {
			snprintf(buf, sizeof(buf), "%d", m);
			out_set(out, outlen, buf);
			return 0;
		}
		if ((rc = rat_mul(&p, &p, &base)) != 0)
			break;
	}
	p = k;
	for (m = 1; m <= 12; m++) {
		if ((rc = rat_mul(&p, &p, &base)) != 0)
			break;
		if (p.n == 1 && p.d == 1) {
			snprintf(buf, sizeof(buf), "%d", -m);
			out_set(out, outlen, buf);
			return 0;
		}
	}
	{
		double v = log((double)k.n / (double)k.d) /
			   log((double)base.n / (double)base.d);
		long long num;

		if (v < -9000000.0 || v > 9000000.0 || v != v || v * 2 == v)
			return -4;
		num = (long long)(v * 1000000000000.0 + (v >= 0 ? 0.5 : -0.5));
		return dec4_render(num, 1000000000000LL, out, outlen);
	}
}

static int solve_log_eval(const char *const *args, char *out, size_t outlen)
{
	rat base, x;
	int rc;
	long long lb, lx;
	char f1[48];

	if ((rc = parse_rat(args[0], &base, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &x, NULL)) != 0)
		return rc;
	if (base.n <= 0 || x.n <= 0 || base.n == base.d)
		return -4;
	lb = log10_9(base.n, base.d, &rc);
	if (rc != 0)
		return rc;
	lx = log10_9(x.n, x.d, &rc);
	if (rc != 0)
		return rc;
	if (lb == 0)
		return -4;
	if (lb < 0) {
		lx = -lx;
		lb = -lb;
	}
	if ((rc = dec4_render(lx, lb, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

static int solve_ln_eval(const char *const *args, char *out, size_t outlen)
{
	rat x;
	int rc;
	long long l;
	char f1[48];

	if ((rc = parse_rat(args[0], &x, NULL)) != 0)
		return rc;
	if (x.n <= 0)
		return -4;
	l = ln_9(x.n, x.d, &rc);
	if (rc != 0)
		return rc;
	if ((rc = dec4_render(l, 1000000000LL, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

static int solve_log10_eval(const char *const *args, char *out, size_t outlen)
{
	rat x;
	int rc;
	long long l;
	char f1[48];

	if ((rc = parse_rat(args[0], &x, NULL)) != 0)
		return rc;
	if (x.n <= 0)
		return -4;
	l = log10_9(x.n, x.d, &rc);
	if (rc != 0)
		return rc;
	if ((rc = dec4_render(l, 1000000000LL, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

static int solve_compound_interest(const char *const *args, char *out,
                                   size_t outlen)
{
	rat P, r, n, t;
	int rc;
	long long lb, x9, e9, num;
	char f1[48];

	if ((rc = parse_rat(args[0], &P, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &r, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &n, NULL)) != 0 ||
	    (rc = parse_rat(args[3], &t, NULL)) != 0)
		return rc;
	if (P.n <= 0 || r.n < 0 || n.n <= 0 || t.n <= 0)
		return -4;
	if (n.d != 1 || t.d != 1 || r.d != 1)
		return -4;
	lb = ln_9(100 * n.n + r.n, 100 * n.n, &rc);
	if (rc != 0)
		return rc;
	x9 = n.n * t.n * lb;
	e9 = exp_9(x9, &rc);
	if (rc != 0)
		return rc;
	if (P.n > 9000000000000000LL / e9)
		return -4;
	num = P.n * e9 / P.d;
	if ((rc = dec4_render(num, 1000000000LL, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

static int solve_continuous_growth(const char *const *args, char *out,
                                   size_t outlen)
{
	rat P, r, t;
	int rc;
	long long x9, e9, num;
	char f1[48];

	if ((rc = parse_rat(args[0], &P, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &r, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &t, NULL)) != 0)
		return rc;
	if (P.n <= 0 || r.n < 0 || t.n <= 0)
		return -4;
	if (r.d != 1 || t.d != 1)
		return -4;
	x9 = r.n * t.n * 10000000LL;
	e9 = exp_9(x9, &rc);
	if (rc != 0)
		return rc;
	if (P.n > 9000000000000000LL / e9)
		return -4;
	num = P.n * e9 / P.d;
	if ((rc = dec4_render(num, 1000000000LL, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

static int solve_half_life(const char *const *args, char *out, size_t outlen)
{
	rat init, hl, t;
	int rc;
	long long x9, e9, num;
	char f1[48];

	if ((rc = parse_rat(args[0], &init, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &hl, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &t, NULL)) != 0)
		return rc;
	if (init.n <= 0 || hl.n <= 0 || t.n < 0)
		return -4;
	if (hl.d != 1 || t.d != 1)
		return -4;
	x9 = -(t.n * 693147181LL) / hl.n;
	e9 = exp_9(x9, &rc);
	if (rc != 0)
		return rc;
	if (e9 <= 0)
		return -4;
	num = init.n * e9 / init.d;
	if ((rc = dec4_render(num, 1000000000LL, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

static int solve_doubling_time(const char *const *args, char *out,
                               size_t outlen)
{
	rat r;
	int rc;
	long long l;
	char f1[48];

	if ((rc = parse_rat(args[0], &r, NULL)) != 0)
		return rc;
	if (r.n <= 0 || r.d != 1)
		return -4;
	l = ln_9(100 + r.n, 100, &rc);
	if (rc != 0)
		return rc;
	if (l <= 0)
		return -4;
	if ((rc = dec4_render(693147181LL, l, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

static int solve_arith_seq(const char *const *args, char *out, size_t outlen)
{
	rat a1, d, n;
	int rc;
	long long an, sn;
	char buf[96];

	if ((rc = parse_rat(args[0], &a1, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &d, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &n, NULL)) != 0)
		return rc;
	if (a1.d != 1 || d.d != 1 || n.d != 1 || n.n < 1)
		return -4;
	an = a1.n + (n.n - 1) * d.n;
	sn = n.n * (a1.n + an) / 2;
	snprintf(buf, sizeof(buf), "a_n=%lld, S_n=%lld", an, sn);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_geom_seq(const char *const *args, char *out, size_t outlen)
{
	rat a1, r, n;
	int rc, i;
	long long an, sn, p;
	char buf[96];

	if ((rc = parse_rat(args[0], &a1, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &r, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &n, NULL)) != 0)
		return rc;
	if (a1.d != 1 || r.d != 1 || n.d != 1 || n.n < 1 || r.n == 1)
		return -4;
	if (n.n > 18)
		return -4;
	p = 1;
	for (i = 1; i < n.n; i++) {
		if (p > 9000000000000000LL / (r.n < 0 ? -r.n : r.n))
			return -4;
		p *= r.n;
	}
	an = a1.n * p;
	if (p > 9000000000000000LL / (r.n < 0 ? -r.n : r.n))
		return -4;
	p *= r.n;
	sn = a1.n * (p - 1) / (r.n - 1);
	snprintf(buf, sizeof(buf), "a_n=%lld, S_n=%lld", an, sn);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_abs_value_eq(const char *const *args, char *out,
                              size_t outlen)
{
	rat a, b, c, r1, r2, t;
	int rc;
	char f1[48], f2[48], buf[112];

	if ((rc = parse_rat(args[0], &a, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &b, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &c, NULL)) != 0)
		return rc;
	if (a.n == 0 || c.n <= 0)
		return -4;
	r1.n = c.n - b.n;
	r1.d = a.n;
	r2.n = -c.n - b.n;
	r2.d = a.n;
	if (r1.d < 0) {
		r1.n = -r1.n;
		r1.d = -r1.d;
	}
	if (r2.d < 0) {
		r2.n = -r2.n;
		r2.d = -r2.d;
	}
	if ((rc = rat_norm(&r1)) != 0 || (rc = rat_norm(&r2)) != 0)
		return rc;
	if (rat_cmp(&r1, &r2) > 0) {
		t = r1;
		r1 = r2;
		r2 = t;
	}
	fmt_rat(&r1, f1, sizeof(f1));
	fmt_rat(&r2, f2, sizeof(f2));
	snprintf(buf, sizeof(buf), "x=%s, x=%s", f1, f2);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_linear_ineq(const char *const *args, char *out,
                             size_t outlen)
{
	rat a, b, c, bound;
	int rc;
	char f1[48], buf[80];

	if ((rc = parse_rat(args[0], &a, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &b, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &c, NULL)) != 0)
		return rc;
	if (a.n == 0)
		return -4;
	bound.n = c.n - b.n;
	bound.d = a.n;
	if (bound.d < 0) {
		bound.n = -bound.n;
		bound.d = -bound.d;
	}
	if ((rc = rat_norm(&bound)) != 0)
		return rc;
	fmt_rat(&bound, f1, sizeof(f1));
	snprintf(buf, sizeof(buf), "x %s %s", a.n > 0 ? "<" : ">", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_complete_square(const char *const *args, char *out,
                                 size_t outlen)
{
	rat a, b, c, h, k, t;
	int rc;
	char f1[48], f2[48], buf[112];

	if ((rc = parse_rat(args[0], &a, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &b, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &c, NULL)) != 0)
		return rc;
	if (a.n == 0)
		return -4;
	h.n = -b.n;
	h.d = 2 * a.n;
	if ((rc = rat_norm(&h)) != 0)
		return rc;
	if ((rc = rat_mul(&t, &b, &b)) != 0)
		return rc;
	t.d *= 4 * a.n;
	if ((rc = rat_norm(&t)) != 0)
		return rc;
	if ((rc = rat_sub(&k, &c, &t)) != 0)
		return rc;
	fmt_rat(&h, f1, sizeof(f1));
	fmt_rat(&k, f2, sizeof(f2));
	snprintf(buf, sizeof(buf), "vertex=(%s, %s)", f1, f2);
	out_set(out, outlen, buf);
	return 0;
}

static void factor_term(long long n, long long d, char *out, size_t outlen)
{
	char f[48];
	rat r;

	r.n = n;
	r.d = d;
	if (rat_norm(&r) != 0) {
		out_set(out, outlen, "");
		return;
	}
	if (r.n == 0) {
		out_set(out, outlen, "x");
		return;
	}
	fmt_rat(&r, f, sizeof(f));
	if (r.n > 0)
		snprintf(out, outlen, "(x-%s)", f);
	else {
		r.n = -r.n;
		fmt_rat(&r, f, sizeof(f));
		snprintf(out, outlen, "(x+%s)", f);
	}
}

static int solve_factor_quadratic(const char *const *args, char *out,
                                  size_t outlen)
{
	rat a, b, c, disc, s, r1, r2, t;
	int rc;
	long long sq;
	char lead[24], f1[64], f2[64], buf[192];

	if ((rc = parse_rat(args[0], &a, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &b, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &c, NULL)) != 0)
		return rc;
	if (a.n == 0)
		return -4;
	if ((rc = rat_mul(&t, &b, &b)) != 0)
		return rc;
	if ((rc = rat_mul(&disc, &a, &c)) != 0)
		return rc;
	disc.n *= 4;
	if ((rc = rat_norm(&disc)) != 0)
		return rc;
	if ((rc = rat_sub(&disc, &t, &disc)) != 0)
		return rc;
	if (disc.n < 0)
		return -4;
	sq = (long long)isqrt64((unsigned long long)disc.n * disc.d);
	if (sq * sq != disc.n * disc.d)
		return -4;
	s.n = sq;
	s.d = disc.d;
	if ((rc = rat_norm(&s)) != 0)
		return rc;
	r1.n = -b.n * s.d + s.n * b.d;
	r1.d = 2 * a.n * s.d * b.d;
	if ((rc = rat_norm(&r1)) != 0)
		return rc;
	r2.n = -b.n * s.d - s.n * b.d;
	r2.d = 2 * a.n * s.d * b.d;
	if ((rc = rat_norm(&r2)) != 0)
		return rc;
	if (rat_cmp(&r1, &r2) > 0) {
		t = r1;
		r1 = r2;
		r2 = t;
	}
	factor_term(r1.n, r1.d, f1, sizeof(f1));
	factor_term(r2.n, r2.d, f2, sizeof(f2));
	if (a.n == 1 && a.d == 1)
		lead[0] = '\0';
	else
		snprintf(lead, sizeof(lead), "%lld", a.n);
	snprintf(buf, sizeof(buf), "%s%s%s", lead, f1, f2);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_cubic_solve(const char *const *args, char *out, size_t outlen)
{
	rat a, b, c, d, r, t, qa, qb, qc, disc, s, r2, r3;
	int rc, found = 0;
	long long p, q, sq;
	char f1[48], f2[48], f3[48], buf[192];

	if ((rc = parse_rat(args[0], &a, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &b, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &c, NULL)) != 0 ||
	    (rc = parse_rat(args[3], &d, NULL)) != 0)
		return rc;
	if (a.n == 0 || a.d != 1 || b.d != 1 || c.d != 1 || d.d != 1)
		return -4;
	if (d.n == 0) {
		r.n = 0;
		r.d = 1;
		found = 1;
	}
	for (p = 1; p <= 60 && !found; p++) {
		for (q = 1; q <= 6 && !found; q++) {
			int sign;

			for (sign = -1; sign <= 1; sign += 2) {
				long long num = sign * p;
				long long v = a.n * num * num * num
				            + b.n * num * num * q
				            + c.n * num * q * q
				            + d.n * q * q * q;

				if (v == 0) {
					r.n = num;
					r.d = q;
					if ((rc = rat_norm(&r)) != 0)
						return rc;
					found = 1;
					break;
				}
			}
		}
	}
	if (!found)
		return -4;
	qa = a;
	if ((rc = rat_mul(&t, &a, &r)) != 0 ||
	    (rc = rat_add(&qb, &b, &t)) != 0 ||
	    (rc = rat_mul(&t, &b, &r)) != 0 ||
	    (rc = rat_add(&qc, &c, &t)) != 0 ||
	    (rc = rat_mul(&t, &a, &r)) != 0 ||
	    (rc = rat_mul(&t, &t, &r)) != 0 ||
	    (rc = rat_add(&qc, &qc, &t)) != 0)
		return rc;
	if ((rc = rat_mul(&disc, &qb, &qb)) != 0 ||
	    (rc = rat_mul(&t, &qa, &qc)) != 0 ||
	    (rc = rat_mul(&t, &t, &RAT_4)) != 0 ||
	    (rc = rat_sub(&disc, &disc, &t)) != 0)
		return rc;
	if (disc.n < 0)
		return -4;
	if (disc.n > WIDE_LIMIT / disc.d)
		return -4;
	sq = (long long)isqrt64((unsigned long long)(disc.n * disc.d));
	if (sq * sq != disc.n * disc.d)
		return -4;
	s.n = sq;
	s.d = disc.d;
	if ((rc = rat_norm(&s)) != 0)
		return rc;
	if ((rc = rat_mul(&t, &qa, &RAT_2)) != 0)
		return rc;
	if ((rc = rat_sub(&r2, &s, &qb)) != 0 ||
	    (rc = rat_div(&r2, &r2, &t)) != 0)
		return rc;
	if ((rc = rat_add(&r3, &qb, &s)) != 0)
		return rc;
	r3.n = -r3.n;
	if ((rc = rat_div(&r3, &r3, &t)) != 0)
		return rc;
	if (rat_cmp(&r2, &r3) > 0) {
		t = r2;
		r2 = r3;
		r3 = t;
	}
	if (rat_cmp(&r, &r2) > 0) {
		t = r;
		r = r2;
		r2 = t;
	}
	if (rat_cmp(&r2, &r3) > 0) {
		t = r2;
		r2 = r3;
		r3 = t;
	}
	fmt_rat(&r, f1, sizeof(f1));
	fmt_rat(&r2, f2, sizeof(f2));
	fmt_rat(&r3, f3, sizeof(f3));
	snprintf(buf, sizeof(buf), "x=%s, x=%s, x=%s", f1, f2, f3);
	out_set(out, outlen, buf);
	return 0;
}

int solvers_math_solve(const char *name, const char *const *args,
                       int nargs, char *out, size_t outlen)
{
	int rc;

	if (strcmp(name, "quad_solve") == 0) {
		if (nargs == 3)
			rc = solve_quad(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "deriv_poly3") == 0) {
		if (nargs == 5)
			rc = solve_deriv(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "integr_poly3") == 0) {
		if (nargs == 6)
			rc = solve_integr(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "linear2") == 0) {
		if (nargs == 6)
			rc = solve_linear2(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "exp_solve") == 0) {
		if (nargs == 2)
			rc = solve_exp(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "log_eval") == 0) {
		if (nargs == 2)
			rc = solve_log_eval(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "ln_eval") == 0) {
		if (nargs == 1)
			rc = solve_ln_eval(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "log10_eval") == 0) {
		if (nargs == 1)
			rc = solve_log10_eval(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "compound_interest") == 0) {
		if (nargs == 4)
			rc = solve_compound_interest(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "continuous_growth") == 0) {
		if (nargs == 3)
			rc = solve_continuous_growth(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "half_life") == 0) {
		if (nargs == 3)
			rc = solve_half_life(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "doubling_time") == 0) {
		if (nargs == 1)
			rc = solve_doubling_time(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "arith_seq") == 0) {
		if (nargs == 3)
			rc = solve_arith_seq(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "geom_seq") == 0) {
		if (nargs == 3)
			rc = solve_geom_seq(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "abs_value_eq") == 0) {
		if (nargs == 3)
			rc = solve_abs_value_eq(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "linear_ineq") == 0) {
		if (nargs == 3)
			rc = solve_linear_ineq(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "complete_square") == 0) {
		if (nargs == 3)
			rc = solve_complete_square(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "factor_quadratic") == 0) {
		if (nargs == 3)
			rc = solve_factor_quadratic(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "cubic_solve") == 0) {
		if (nargs == 4)
			rc = solve_cubic_solve(args, out, outlen);
		else
			rc = -2;
	} else {
		return 0;
	}
	return rc == 0 ? 1 : rc;
}
