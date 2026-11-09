#include <stdio.h>
#include <string.h>

#include "solvers.h"
#include "solvers_subject.h"

static int solve_sin_eval(const char *const *args, char *out, size_t outlen)
{
	rat a;
	int rc, ea;
	long long s9;
	char f1[48];

	if ((rc = parse_rat(args[0], &a, NULL)) != 0)
		return rc;
	ea = pow10_index(a.d);
	if (ea < 0 || ea > 9)
		return -4;
	s9 = sin9(a.n, ea, &rc);
	if (rc != 0)
		return rc;
	if ((rc = dec4_render(s9, 1000000000LL, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

static int solve_cos_eval(const char *const *args, char *out, size_t outlen)
{
	rat a;
	int rc, ea;
	long long s9;
	char f1[48];

	if ((rc = parse_rat(args[0], &a, NULL)) != 0)
		return rc;
	ea = pow10_index(a.d);
	if (ea < 0 || ea > 9)
		return -4;
	s9 = sin9(a.n + 90LL * P10[ea], ea, &rc);
	if (rc != 0)
		return rc;
	if ((rc = dec4_render(s9, 1000000000LL, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

static int solve_tan_eval(const char *const *args, char *out, size_t outlen)
{
	rat a;
	int rc, ea;
	long long s9, c9;
	char f1[48];

	if ((rc = parse_rat(args[0], &a, NULL)) != 0)
		return rc;
	ea = pow10_index(a.d);
	if (ea < 0 || ea > 9)
		return -4;
	s9 = sin9(a.n, ea, &rc);
	if (rc != 0)
		return rc;
	c9 = sin9(a.n + 90LL * P10[ea], ea, &rc);
	if (rc != 0)
		return rc;
	if (c9 == 0)
		return -4;
	if (c9 < 0) {
		s9 = -s9;
		c9 = -c9;
	}
	if ((rc = dec4_render(s9, c9, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

static int solve_pythagoras(const char *const *args, char *out, size_t outlen)
{
	rat a, b, s;
	int rc;
	long long s12;
	char f1[48];

	if ((rc = parse_rat(args[0], &a, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &b, NULL)) != 0)
		return rc;
	s.n = a.n * a.n * b.d * b.d + b.n * b.n * a.d * a.d;
	s.d = a.d * a.d * b.d * b.d;
	if ((rc = rat_norm(&s)) != 0)
		return rc;
	s12 = sqrt12(s.n, s.d, &rc);
	if (rc != 0)
		return rc;
	if ((rc = dec4_render(s12, 1000000000000LL, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

static int solve_law_sines(const char *const *args, char *out, size_t outlen)
{
	rat a, A, B;
	int rc, eA, eB;
	long long sA, sB, num, den;
	char f1[48];

	if ((rc = parse_rat(args[0], &a, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &A, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &B, NULL)) != 0)
		return rc;
	eA = pow10_index(A.d);
	eB = pow10_index(B.d);
	if (eA < 0 || eA > 9 || eB < 0 || eB > 9)
		return -4;
	sA = sin9(A.n, eA, &rc);
	if (rc != 0)
		return rc;
	sB = sin9(B.n, eB, &rc);
	if (rc != 0)
		return rc;
	if (sA == 0)
		return -4;
	num = a.n * sB;
	den = a.d * sA;
	if (den < 0) {
		num = -num;
		den = -den;
	}
	if ((rc = dec4_render(num, den, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

static int solve_law_cosines(const char *const *args, char *out, size_t outlen)
{
	rat a, b, C;
	int rc, eC;
	long long sC, num, den, c2_9, c6, rr;
	char f1[48];

	if ((rc = parse_rat(args[0], &a, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &b, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &C, NULL)) != 0)
		return rc;
	eC = pow10_index(C.d);
	if (eC < 0 || eC > 9)
		return -4;
	sC = sin9(C.n + 90LL * P10[eC], eC, &rc);
	if (rc != 0)
		return rc;
	num = (a.n * a.n * b.d * b.d + b.n * b.n * a.d * a.d) * 1000000000LL
	      - 2 * a.n * b.n * a.d * b.d * sC;
	den = a.d * a.d * b.d * b.d * 1000000000LL;
	if (num <= 0 || den <= 0)
		return -4;
	c2_9 = (num / den) * 1000000000LL + ((num % den) * 1000000000LL) / den;
	if (c2_9 <= 0)
		return -4;
	c6 = (long long)isqrt64((unsigned long long)c2_9 * 1000ULL);
	rr = c2_9 * 1000LL - c6 * c6;
	c6 += div_round(rr, 2 * c6);
	if ((rc = dec4_render(c6, 1000000LL, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

static int solve_arc_length(const char *const *args, char *out, size_t outlen)
{
	rat r, th;
	int rc;
	long long num, den;
	char f1[48];

	if ((rc = parse_rat(args[0], &r, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &th, NULL)) != 0)
		return rc;
	num = r.n * th.n * 3141592653LL;
	den = r.d * th.d * 180LL * 1000000000LL;
	if (den < 0) {
		num = -num;
		den = -den;
	}
	if ((rc = dec4_render(num, den, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

/*
 * atan(num/den) in degrees at 1e9 scale, for num, den > 0.
 * Reduces to the first octant, then applies atan(x) = 2*atan(x/(1+sqrt(1+x^2)))
 * twice so the Taylor series converges quickly, then converts to degrees.
 */
static long long atan_deg9(long long num, long long den, int *rc)
{
	long long x9, x2, t9, sum9, root9;
	int i, k;

	if (num <= 0 || den <= 0) {
		*rc = -4;
		return 0;
	}
	if (num > den) {
		long long r = atan_deg9(den, num, rc);

		if (*rc != 0)
			return 0;
		return 90000000000LL - r;
	}
	if (num > 9000000000000000LL / 1000000000LL) {
		*rc = -4;
		return 0;
	}
	x9 = num * 1000000000LL / den;
	for (k = 0; k < 2; k++) {
		x2 = x9 * x9 / 1000000000LL;
		if (x2 > 9000000000LL) {
			*rc = -4;
			return 0;
		}
		root9 = (long long)isqrt64((unsigned long long)(1000000000000000000LL
		                                              + x2 * 1000000000LL));
		x9 = x9 * 1000000000LL / (1000000000LL + root9);
	}
	x2 = x9 * x9 / 1000000000LL;
	t9 = x9;
	sum9 = x9;
	for (i = 1; i <= 15; i++) {
		t9 = -t9 * x2 / 1000000000LL;
		if (t9 == 0)
			break;
		sum9 += t9 / (2 * i + 1);
	}
	sum9 *= 4;
	return div_round(sum9 * 180LL * 1000000LL, 3141592653LL) * 1000;
}

static int solve_sinusoid(const char *const *args, char *out, size_t outlen)
{
	rat A, B, C, period, shift;
	int rc;
	char f1[48], f2[48], f3[48], buf[192];

	if ((rc = parse_rat(args[0], &A, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &B, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &C, NULL)) != 0)
		return rc;
	if (B.n == 0)
		return -4;
	period.n = 360 * B.d;
	period.d = (B.n < 0 ? -B.n : B.n);
	if ((rc = rat_norm(&period)) != 0)
		return rc;
	shift.n = -C.n * B.d;
	shift.d = C.d * B.n;
	if (shift.d < 0) {
		shift.n = -shift.n;
		shift.d = -shift.d;
	}
	if ((rc = rat_norm(&shift)) != 0)
		return rc;
	A.n = A.n < 0 ? -A.n : A.n;
	fmt_rat(&A, f1, sizeof(f1));
	fmt_rat(&period, f2, sizeof(f2));
	fmt_rat(&shift, f3, sizeof(f3));
	snprintf(buf, sizeof(buf), "amp=%s, period=%s, shift=%s", f1, f2, f3);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_right_triangle(const char *const *args, char *out,
                                size_t outlen)
{
	rat opp, adj, s;
	int rc;
	long long ang9, s12;
	char f1[48], f2[48], buf[112];

	if ((rc = parse_rat(args[0], &opp, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &adj, NULL)) != 0)
		return rc;
	if (opp.n <= 0 || adj.n <= 0)
		return -4;
	ang9 = atan_deg9(opp.n * adj.d, adj.n * opp.d, &rc);
	if (rc != 0)
		return rc;
	s.n = opp.n * opp.n * adj.d * adj.d + adj.n * adj.n * opp.d * opp.d;
	s.d = opp.d * opp.d * adj.d * adj.d;
	if ((rc = rat_norm(&s)) != 0)
		return rc;
	s12 = sqrt12(s.n, s.d, &rc);
	if (rc != 0)
		return rc;
	if ((rc = dec4_render(ang9, 1000000000LL, f1, sizeof(f1))) != 0)
		return rc;
	if ((rc = dec4_render(s12, 1000000000000LL, f2, sizeof(f2))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "angle=%s, hyp=%s", f1, f2);
	out_set(out, outlen, buf);
	return 0;
}

int solvers_trig_solve(const char *name, const char *const *args,
                       int nargs, char *out, size_t outlen)
{
	int rc;

	if (strcmp(name, "sin_eval") == 0) {
		if (nargs == 1)
			rc = solve_sin_eval(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "cos_eval") == 0) {
		if (nargs == 1)
			rc = solve_cos_eval(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "tan_eval") == 0) {
		if (nargs == 1)
			rc = solve_tan_eval(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "pythagoras") == 0) {
		if (nargs == 2)
			rc = solve_pythagoras(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "law_sines") == 0) {
		if (nargs == 3)
			rc = solve_law_sines(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "law_cosines") == 0) {
		if (nargs == 3)
			rc = solve_law_cosines(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "arc_length") == 0) {
		if (nargs == 2)
			rc = solve_arc_length(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "sinusoid") == 0) {
		if (nargs == 3)
			rc = solve_sinusoid(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "right_triangle") == 0) {
		if (nargs == 2)
			rc = solve_right_triangle(args, out, outlen);
		else
			rc = -2;
	} else {
		return 0;
	}
	return rc == 0 ? 1 : rc;
}
