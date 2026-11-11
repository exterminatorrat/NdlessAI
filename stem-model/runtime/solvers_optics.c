/*
 * Optics: refraction, the lens and mirror equations, magnification.
 *
 * Angles arrive and leave in degrees, the way the questions are written, with
 * no unit on the answer - the same convention the trigonometry solvers use.
 *
 * The inverse sine a refracted angle needs is not a routine of its own.
 * asin(x) is atan(x/sqrt(1-x^2)), and atan is already in the shared boundary,
 * so there is one inverse-trig implementation in the runtime rather than two
 * that can disagree on the last digit.
 */
#include <stdio.h>
#include <string.h>

#include "solvers.h"
#include "solvers_subject.h"

/* An index, an angle or a distance the fixed point can hold without the
 * intermediate products below overflowing. */
static int rat_sane(const rat *r)
{
	return r->n >= 0 && r->n <= 1000000 && r->d > 0 && r->d <= 1000000;
}

/* v at scale 1e9, or a negative code when the ratio is out of range. */
static long long to9(const rat *r)
{
	if (!rat_sane(r))
		return -4;
	return div_round(r->n * 1000000000LL, r->d);
}

/* The angle whose sine is s9/1e9, in degrees at scale 1e9. */
static long long asin_deg9(long long s9, int *rc)
{
	long long c2, c9;

	if (s9 <= 0 || s9 >= 1000000000LL) {
		*rc = -4;
		return 0;
	}
	/* cos^2 at 1e18, which is what isqrt64 turns back into 1e9-scaled
	 * cosine; multiplying at 1e9 here instead overflows for a sine near 1. */
	c2 = 1000000000000000000LL - s9 * s9;
	if (c2 <= 0) {
		*rc = -4;
		return 0;
	}
	c9 = (long long)isqrt64((unsigned long long)c2);
	if (c9 <= 0) {
		*rc = -4;
		return 0;
	}
	return atan_deg9(s9, c9, rc);
}

static int solve_snell_angle(const char *const *args, char *out, size_t outlen)
{
	rat n1, th1, n2;
	long long n19, n29, s9, x9, a9;
	int rc, e1;
	char f1[48];

	if ((rc = parse_rat(args[0], &n1, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &th1, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &n2, NULL)) != 0)
		return rc;
	if (th1.n <= 0 || th1.n >= 90000000000LL)
		return -4;
	e1 = pow10_index(th1.d);
	if (e1 < 0 || e1 > 9)
		return -4;
	n19 = to9(&n1);
	n29 = to9(&n2);
	if (n19 <= 0 || n29 <= 0)
		return -4;
	s9 = sin9(th1.n, e1, &rc);
	if (rc != 0)
		return rc;
	/* n1*sin(th1) = n2*sin(th2), so sin(th2) is that ratio. */
	x9 = div_round(s9 * n19, n29);
	if (x9 <= 0 || x9 >= 1000000000LL)
		return -4;	/* past the critical angle there is no refracted ray */
	a9 = asin_deg9(x9, &rc);
	if (rc != 0)
		return rc;
	if ((rc = dec4_render(a9, 1000000000LL, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

static int solve_critical_angle(const char *const *args, char *out,
                                size_t outlen)
{
	rat n1, n2;
	long long n19, n29, a9;
	int rc;
	char f1[48];

	if ((rc = parse_rat(args[0], &n1, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &n2, NULL)) != 0)
		return rc;
	n19 = to9(&n1);
	n29 = to9(&n2);
	if (n19 <= 0 || n29 <= 0 || n29 >= n19)
		return -4;	/* no critical angle unless the ray can be trapped */
	/* sin(thc) = n2/n1, at 1e9 scale. */
	a9 = asin_deg9(n29 * 1000000000LL / n19, &rc);
	if (rc != 0)
		return rc;
	if ((rc = dec4_render(a9, 1000000000LL, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

/* di from 1/f = 1/do + 1/di, which is di = f*do/(do - f). */
static int image_distance(const char *const *args, char *out, size_t outlen)
{
	rat f, do_, num, den, di;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &f, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &do_, NULL)) != 0)
		return rc;
	if (!rat_sane(&f) || !rat_sane(&do_) || f.n == 0)
		return -4;
	if ((rc = rat_mul(&num, &f, &do_)) != 0 ||
	    (rc = rat_sub(&den, &do_, &f)) != 0)
		return rc;
	if (den.n == 0)
		return -4;	/* the object sits at the focus */
	if ((rc = rat_div(&di, &num, &den)) != 0)
		return rc;
	if ((rc = dec4_render(di.n, di.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s cm", f1);
	out_set(out, outlen, buf);
	return 0;
}

/* m = -di/do. */
static int solve_lens_magnification(const char *const *args, char *out,
                                    size_t outlen)
{
	rat di, do_, m;
	int rc;
	char f1[48];

	if ((rc = parse_rat(args[0], &di, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &do_, NULL)) != 0)
		return rc;
	if (!rat_sane(&di) || !rat_sane(&do_) || do_.n == 0)
		return -4;
	if ((rc = rat_div(&m, &di, &do_)) != 0)
		return rc;
	m.n = -m.n;
	if ((rc = dec4_render(m.n, m.d, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

/* n = 3/v, with v stated in units of 1e8 m/s so the answer is c/v exactly. */
static int solve_index_from_speed(const char *const *args, char *out,
                                  size_t outlen)
{
	rat v, c, n;
	int rc;
	char f1[48];

	if ((rc = parse_rat(args[0], &v, NULL)) != 0)
		return rc;
	if (!rat_sane(&v) || v.n == 0)
		return -4;
	c.n = 3;
	c.d = 1;
	if ((rc = rat_div(&n, &c, &v)) != 0)
		return rc;
	if ((rc = dec4_render(n.n, n.d, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

int solvers_optics_solve(const char *name, const char *const *args, int nargs,
                         char *out, size_t outlen)
{
	int rc;

	if (strcmp(name, "snell_angle") == 0) {
		if (nargs == 3)
			rc = solve_snell_angle(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "critical_angle") == 0) {
		if (nargs == 2)
			rc = solve_critical_angle(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "lens_image_distance") == 0) {
		if (nargs == 2)
			rc = image_distance(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "mirror_image_distance") == 0) {
		if (nargs == 2)
			rc = image_distance(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "lens_magnification") == 0) {
		if (nargs == 2)
			rc = solve_lens_magnification(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "index_from_speed") == 0) {
		if (nargs == 1)
			rc = solve_index_from_speed(args, out, outlen);
		else
			rc = -2;
	} else {
		return 0;
	}
	return rc == 0 ? 1 : rc;
}
