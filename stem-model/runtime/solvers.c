#include <math.h>
#include <stdio.h>
#include <string.h>

#include "solvers.h"

#define OP_LIMIT    2000000000LL
#define WIDE_LIMIT  9000000000000000LL
#define DEC_DEN_MAX 900000000000000000LL

typedef struct {
	long long n, d;
} rat;

static const rat RAT_2 = { 2, 1 };
static const rat RAT_3 = { 3, 1 };
static const rat RAT_4 = { 4, 1 };

static const long long P10[] = {
	1LL, 10LL, 100LL, 1000LL, 10000LL, 100000LL, 1000000LL,
	10000000LL, 100000000LL, 1000000000LL, 10000000000LL,
	100000000000LL, 1000000000000LL, 10000000000000LL,
	100000000000000LL, 1000000000000000LL, 10000000000000000LL,
	100000000000000000LL, 1000000000000000000LL
};

static long long gcd_ll(long long a, long long b)
{
	if (a < 0)
		a = -a;
	if (b < 0)
		b = -b;
	while (b != 0) {
		long long t = a % b;

		a = b;
		b = t;
	}
	return a;
}

static int rat_norm(rat *r)
{
	long long g;

	if (r->d <= 0 || r->n > OP_LIMIT || -r->n > OP_LIMIT ||
	    r->d > OP_LIMIT)
		return -4;
	if (r->n == 0) {
		r->d = 1;
		return 0;
	}
	g = gcd_ll(r->n, r->d);
	r->n /= g;
	r->d /= g;
	return 0;
}

static int rat_small(const rat *r)
{
	return r->n <= OP_LIMIT && -r->n <= OP_LIMIT && r->d <= OP_LIMIT;
}

static int rat_add(rat *r, const rat *a, const rat *b)
{
	if (!rat_small(a) || !rat_small(b))
		return -4;
	r->n = a->n * b->d + b->n * a->d;
	r->d = a->d * b->d;
	return rat_norm(r);
}

static int rat_sub(rat *r, const rat *a, const rat *b)
{
	if (!rat_small(a) || !rat_small(b))
		return -4;
	r->n = a->n * b->d - b->n * a->d;
	r->d = a->d * b->d;
	return rat_norm(r);
}

static int rat_mul(rat *r, const rat *a, const rat *b)
{
	if (!rat_small(a) || !rat_small(b))
		return -4;
	r->n = a->n * b->n;
	r->d = a->d * b->d;
	return rat_norm(r);
}

static int rat_div(rat *r, const rat *a, const rat *b)
{
	if (!rat_small(a) || !rat_small(b) || b->n == 0)
		return -4;
	r->n = a->n * b->d;
	r->d = a->d * b->n;
	if (r->d < 0) {
		r->d = -r->d;
		r->n = -r->n;
	}
	return rat_norm(r);
}

static int rat_cmp(const rat *a, const rat *b)
{
	long long l = a->n * b->d, r = b->n * a->d;

	return (l > r) - (l < r);
}

static unsigned long long isqrt64(unsigned long long n)
{
	unsigned long long x, y;

	if (n == 0)
		return 0;
	x = n;
	for (;;) {
		y = (x + n / x) / 2;
		if (y >= x)
			break;
		x = y;
	}
	while (x > n / x)
		x--;
	while (x + 1 <= n / (x + 1))
		x++;
	return x;
}

static long long div_round(long long a, long long b)
{
	if (a >= 0)
		return (a + b / 2) / b;
	return -((-a + b / 2) / b);
}

static int pow10_index(long long d)
{
	int i;

	for (i = 0; i < 19; i++)
		if (P10[i] == d)
			return i;
	return -1;
}

static void out_clear(char *out, size_t outlen)
{
	if (out != NULL && outlen > 0)
		out[0] = '\0';
}

static void out_set(char *out, size_t outlen, const char *buf)
{
	size_t n = strlen(buf);

	if (out == NULL || outlen == 0)
		return;
	if (n >= outlen)
		n = outlen - 1;
	memcpy(out, buf, n);
	out[n] = '\0';
}

/*
 * Four significant digits of the exact rational num/den (den > 0), round
 * half to even on the exact value, trailing zeros stripped, %.4g
 * fixed/scientific selection.
 */
static int dec4_render(long long num, long long den, char *out, size_t outlen)
{
	char buf[48], tmp[32], *w;
	long long un, q, rem, t;
	size_t m, g;
	int d[5], neg = 0, e, k, len, i, up, more, nd, fz;

	if (den <= 0 || den > DEC_DEN_MAX || num > WIDE_LIMIT ||
	    -num > WIDE_LIMIT)
		return -4;
	if (num == 0) {
		out_set(out, outlen, "0");
		return 0;
	}
	if (num < 0) {
		neg = 1;
		un = -num;
	} else {
		un = num;
	}
	g = (size_t)gcd_ll(un, den);
	un /= (long long)g;
	den /= (long long)g;
	more = 0;
	if (un >= den) {
		long long p = 1;

		k = 0;
		while (p <= un / (den * 10)) {
			p *= 10;
			k++;
		}
		e = k;
		q = un / den;
		rem = un % den;
		snprintf(tmp, sizeof(tmp), "%lld", q);
		len = (int)strlen(tmp);
		nd = len < 5 ? len : 5;
		for (i = 0; i < nd; i++)
			d[i] = tmp[i] - '0';
		if (len > 5) {
			for (i = 5; tmp[i] != '\0'; i++)
				if (tmp[i] != '0')
					more = 1;
		} else {
			for (i = len; i < 5; i++) {
				if (rem == 0) {
					d[i] = 0;
					continue;
				}
				rem *= 10;
				d[i] = (int)(rem / den);
				rem %= den;
			}
			more = rem != 0;
		}
	} else {
		t = un;
		e = 0;
		while (t < den) {
			t *= 10;
			e--;
		}
		d[0] = (int)(t / den);
		rem = t % den;
		for (i = 1; i < 5; i++) {
			if (rem == 0) {
				d[i] = 0;
				continue;
			}
			rem *= 10;
			d[i] = (int)(rem / den);
			rem %= den;
		}
		more = rem != 0;
	}
	up = 0;
	if (d[4] > 5 || (d[4] == 5 && more))
		up = 1;
	else if (d[4] == 5)
		up = d[3] & 1;
	if (up) {
		i = 3;
		while (i >= 0 && d[i] == 9)
			d[i--] = 0;
		if (i < 0) {
			d[0] = 1;
			d[1] = 0;
			d[2] = 0;
			d[3] = 0;
			e++;
		} else {
			d[i]++;
		}
	}
	w = buf;
	if (e >= 4 || e <= -5) {
		*w++ = (char)('0' + d[0]);
		fz = 3;
		while (fz > 0 && d[fz] == 0)
			fz--;
		if (fz > 0) {
			*w++ = '.';
			for (i = 1; i <= fz; i++)
				*w++ = (char)('0' + d[i]);
		}
		*w++ = 'e';
		*w++ = e < 0 ? '-' : '+';
		k = e < 0 ? -e : e;
		if (k >= 100) {
			*w++ = (char)('0' + k / 100);
			*w++ = (char)('0' + k / 10 % 10);
			*w++ = (char)('0' + k % 10);
		} else {
			*w++ = (char)('0' + k / 10);
			*w++ = (char)('0' + k % 10);
		}
	} else if (e >= 0) {
		for (i = 0; i <= e; i++)
			*w++ = (char)('0' + d[i]);
		fz = 3;
		while (fz > e && d[fz] == 0)
			fz--;
		if (fz > e) {
			*w++ = '.';
			for (i = e + 1; i <= fz; i++)
				*w++ = (char)('0' + d[i]);
		}
	} else {
		*w++ = '0';
		*w++ = '.';
		for (i = 0; i < -e - 1; i++)
			*w++ = '0';
		fz = 3;
		while (fz > 0 && d[fz] == 0)
			fz--;
		for (i = 0; i <= fz; i++)
			*w++ = (char)('0' + d[i]);
	}
	*w = '\0';
	w = tmp;
	if (neg)
		*w++ = '-';
	for (m = 0; buf[m] != '\0'; m++)
		*w++ = buf[m];
	*w = '\0';
	out_set(out, outlen, tmp);
	return 0;
}

int solvers_dec4(long long num, long long den, char *out, size_t outlen)
{
	int rc;

	rc = dec4_render(num, den, out, outlen);
	if (rc != 0)
		out_clear(out, outlen);
	return rc;
	}

static void fmt_rat(const rat *r, char *out, size_t outlen)
{
	char buf[48];

	if (r->d == 1)
		snprintf(buf, sizeof(buf), "%lld", r->n);
	else
		snprintf(buf, sizeof(buf), "%lld/%lld", r->n, r->d);
	out_set(out, outlen, buf);
}

static int parse_rat(const char *s, rat *r, int *flt)
{
	long long num = 0;
	int neg = 0, exp = 0, digits = 0, dot = 0;
	const char *p = s;

	if (*p == '-') {
		neg = 1;
		p++;
	}
	for (; *p >= '0' && *p <= '9'; p++) {
		num = num * 10 + (*p - '0');
		digits++;
		if (num > WIDE_LIMIT / 10)
			return -3;
	}
	if (*p == '.') {
		p++;
		dot = 1;
		for (; *p >= '0' && *p <= '9'; p++) {
			num = num * 10 + (*p - '0');
			exp++;
			digits++;
			if (num > WIDE_LIMIT / 10 || exp > 9)
				return -3;
		}
	}
	if (digits == 0 || *p != '\0')
		return -3;
	if (flt != NULL)
		*flt = dot;
	while (exp > 0 && num % 10 == 0) {
		num /= 10;
		exp--;
	}
	r->n = neg ? -num : num;
	r->d = P10[exp];
	return 0;
}

/*
 * sin(theta) for theta in degrees, at scale 1e9: scaled range reduction,
 * then Taylor series (sin through x^13, cos through x^14).
 */
static long long sin9(long long tnum, int texp, int *rc)
{
	const long long PI9 = 3141592653LL;
	const long long TAU9 = 6283185306LL;
	const long long HALF_PI9 = 1570796326LL;
	const long long QUART_PI9 = 785398163LL;
	long long num, den, x, q, t, s, sgn;
	int k;

	if (tnum > 2900000000LL || -tnum > 2900000000LL || texp > 9) {
		*rc = -4;
		return 0;
	}
	num = tnum * PI9;
	den = 180LL * P10[texp];
	x = num / den;
	x %= TAU9;
	if (x < 0)
		x += TAU9;
	sgn = 1;
	if (x > PI9) {
		x -= PI9;
		sgn = -1;
	}
	if (x > HALF_PI9)
		x = PI9 - x;
	if (x > QUART_PI9) {
		long long y = HALF_PI9 - x;

		q = div_round(y * y, 1000000000LL);
		t = 1000000000LL;
		s = 1000000000LL;
		for (k = 1; k <= 7; k++) {
			t = -div_round(t * q,
				       (2 * k - 1) * 2 * k * 1000000000LL);
			s += t;
			if (t == 0)
				break;
		}
	} else {
		q = div_round(x * x, 1000000000LL);
		t = x;
		s = x;
		for (k = 2; k <= 12; k += 2) {
			t = -div_round(t * q, k * (k + 1) * 1000000000LL);
			s += t;
			if (t == 0)
				break;
		}
	}
	return sgn * s;
}

/*
 * sqrt(n/d) at scale 1e12 for reduced n,d > 0, via isqrt plus two Newton
 * correction terms; supports n*d up to 4e6 (covers the generator domain).
 */
static long long sqrt12(long long n, long long d, int *rc)
{
	unsigned long long q, s3, r;
	long long r1, r2;

	if (d <= 0 || n > WIDE_LIMIT / d) {
		*rc = -4;
		return 0;
	}
	q = (unsigned long long)n * (unsigned long long)d;
	if (q > 4000000ULL) {
		*rc = -4;
		return 0;
	}
	s3 = isqrt64(q * 1000000ULL);
	r = q * 1000000ULL - s3 * s3;
	r1 = div_round((long long)r * 1000000000LL, 2 * (long long)s3);
	r2 = div_round(div_round((long long)r * (long long)r,
				 8 * (long long)s3) * 1000000000LL,
		       (long long)s3 * (long long)s3);
	return div_round((long long)s3 * 1000000000LL + r1 - r2, d);
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

static int solve_exp(const char *const *args, char *out, size_t outlen)
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

static int solve_kinematic(const char *const *args, char *out, size_t outlen)
{
	rat v0, a, t, at, r;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &v0, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &a, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &t, NULL)) != 0)
		return rc;
	if ((rc = rat_mul(&at, &a, &t)) != 0 ||
	    (rc = rat_add(&r, &v0, &at)) != 0)
		return rc;
	if ((rc = dec4_render(r.n, r.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s m/s", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_proj(const char *const *args, char *out, size_t outlen)
{
	rat v0, th, g;
	int rc, ev, eg, et;
	long long s9, num, den;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &v0, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &th, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &g, NULL)) != 0)
		return rc;
	ev = pow10_index(v0.d);
	et = pow10_index(th.d);
	eg = pow10_index(g.d);
	if (ev < 0 || et < 0 || eg < 0 || ev > 1 || eg > 2)
		return -4;
	if (v0.n > 3900LL || -v0.n > 3900LL)
		return -4;
	if (g.n > 9200000LL || -g.n > 9200000LL)
		return -4;
	s9 = sin9(2 * th.n, et, &rc);
	if (rc != 0)
		return rc;
	num = (v0.n * v0.n) * s9;
	num *= P10[eg];
	den = g.n * 1000000000LL * P10[2 * ev];
	if (den < 0) {
		den = -den;
		num = -num;
	}
	if ((rc = dec4_render(num, den, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s m", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_ohm(const char *const *args, char *out, size_t outlen)
{
	rat v, r, q;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &v, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &r, NULL)) != 0)
		return rc;
	if ((rc = rat_div(&q, &v, &r)) != 0)
		return rc;
	if ((rc = dec4_render(q.n, q.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s A", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_moles(const char *const *args, char *out, size_t outlen)
{
	rat mass, mm, q;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &mass, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &mm, NULL)) != 0)
		return rc;
	if (mm.n == 0)
		return -4;
	if ((rc = rat_div(&q, &mass, &mm)) != 0)
		return rc;
	if ((rc = dec4_render(q.n, q.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s mol", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_dilution(const char *const *args, char *out, size_t outlen)
{
	rat c1, v1, v2, t, q;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &c1, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &v1, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &v2, NULL)) != 0)
		return rc;
	if (v2.n == 0)
		return -4;
	if ((rc = rat_mul(&t, &c1, &v1)) != 0 ||
	    (rc = rat_div(&q, &t, &v2)) != 0)
		return rc;
	if ((rc = dec4_render(q.n, q.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s M", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_gasp(const char *const *args, char *out, size_t outlen)
{
	rat n, t, v, big_r, t1, p;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &n, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &t, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &v, NULL)) != 0)
		return rc;
	if (v.n == 0)
		return -4;
	big_r.n = 8206;
	big_r.d = 100000;
	if ((rc = rat_mul(&t1, &n, &big_r)) != 0 ||
	    (rc = rat_mul(&t1, &t1, &t)) != 0 ||
	    (rc = rat_div(&p, &t1, &v)) != 0)
		return rc;
	if ((rc = dec4_render(p.n, p.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s atm", f1);
	out_set(out, outlen, buf);
	return 0;
}

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
 * atanh((m-1)/(m+1)) at 1e9 scale, for x = num/den > 0 written as m*10^k
 * with m in [0.316, 3.163). The series converges in about 20 terms there.
 * Callers combine k and the returned sum with their own base constant.
 */
static long long log_core_9(long long num, long long den, long long *k_out,
                            int *rc)
{
	long long k = 0, x9, z9, z2_9, t9, s9;
	int i;

	if (num <= 0 || den <= 0 || den > 1000000000LL) {
		*rc = -4;
		return 0;
	}
	if (num / den > 1000000LL) {
		*rc = -4;
		return 0;
	}
	x9 = (num / den) * 1000000000LL + ((num % den) * 1000000000LL) / den;
	if (x9 <= 0) {
		*rc = -4;
		return 0;
	}
	while (x9 >= 10000000000LL) {
		x9 /= 10;
		k++;
	}
	while (x9 < 1000000000LL) {
		x9 *= 10;
		k--;
	}
	if (x9 >= 3162277660LL) {
		x9 /= 10;
		k++;
	}
	z9 = (x9 - 1000000000LL) * 1000000000LL / (x9 + 1000000000LL);
	z2_9 = z9 * z9 / 1000000000LL;
	t9 = z9;
	s9 = z9;
	for (i = 3; i <= 41; i += 2) {
		t9 = t9 * z2_9 / 1000000000LL;
		if (t9 == 0)
			break;
		s9 += t9 / i;
	}
	*k_out = k;
	return s9;
}

static long long log10_9(long long num, long long den, int *rc)
{
	long long k = 0, s9 = log_core_9(num, den, &k, rc);

	if (*rc != 0)
		return 0;
	return k * 1000000000LL + s9 * 868588964LL / 1000000000LL;
}

static long long ln_9(long long num, long long den, int *rc)
{
	long long k = 0, s9 = log_core_9(num, den, &k, rc);

	if (*rc != 0)
		return 0;
	return k * 2302585093LL + 2 * s9;
}

/*
 * e^x at 1e9 scale for x = x9/1e9.
 * Splits into an integer part and a fraction in [-0.5, 0.5]; the fraction
 * uses a Taylor series that converges in about 12 terms there.
 */
static long long exp_9(long long x9, int *rc)
{
	long long n, f9, t9, s9, r9;
	int i;

	if (x9 > 20000000000LL || x9 < -20000000000LL) {
		*rc = -4;
		return 0;
	}
	n = x9 / 1000000000LL;
	f9 = x9 - n * 1000000000LL;
	if (f9 > 500000000LL) {
		f9 -= 1000000000LL;
		n++;
	} else if (f9 < -500000000LL) {
		f9 += 1000000000LL;
		n--;
	}
	t9 = 1000000000LL;
	s9 = 1000000000LL;
	for (i = 1; i <= 20; i++) {
		t9 = div_round(div_round(t9 * f9, 1000000000LL), i);
		if (t9 == 0)
			break;
		s9 += t9;
	}
	r9 = 1000000000LL;
	if (n > 0) {
		for (i = 0; i < n; i++) {
			if (r9 > 3000000000000000LL) {
				*rc = -4;
				return 0;
			}
			r9 = div_round(r9 * 2718281828LL, 1000000000LL);
		}
	} else {
		for (i = 0; i < -n; i++)
			r9 = div_round(r9 * 1000000000LL, 2718281828LL);
	}
	if (r9 == 0 || s9 > 9000000000000000000LL / r9) {
		*rc = -4;
		return 0;
	}
	return div_round(s9 * r9, 1000000000LL);
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

static int solve_kinetic_energy(const char *const *args, char *out,
                                size_t outlen)
{
	rat m, v;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &m, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &v, NULL)) != 0)
		return rc;
	if ((rc = dec4_render(m.n * v.n * v.n, 2 * m.d * v.d * v.d,
	                      f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s J", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_potential_energy(const char *const *args, char *out,
                                  size_t outlen)
{
	rat m, h, g;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &m, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &h, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &g, NULL)) != 0)
		return rc;
	if ((rc = dec4_render(m.n * h.n * g.n, m.d * h.d * g.d,
	                      f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s J", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_work(const char *const *args, char *out, size_t outlen)
{
	rat F, d, th;
	int rc, et;
	long long c9, num, den;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &F, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &d, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &th, NULL)) != 0)
		return rc;
	et = pow10_index(th.d);
	if (et < 0 || et > 9)
		return -4;
	c9 = sin9(th.n + 90LL * P10[et], et, &rc);
	if (rc != 0)
		return rc;
	num = F.n * d.n * c9;
	den = F.d * d.d * 1000000000LL;
	if (den < 0) {
		num = -num;
		den = -den;
	}
	if ((rc = dec4_render(num, den, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s J", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_power(const char *const *args, char *out, size_t outlen)
{
	rat W, t;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &W, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &t, NULL)) != 0)
		return rc;
	if (t.n == 0)
		return -4;
	if ((rc = dec4_render(W.n * t.d, W.d * t.n, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s W", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_momentum(const char *const *args, char *out, size_t outlen)
{
	rat m, v;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &m, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &v, NULL)) != 0)
		return rc;
	if ((rc = dec4_render(m.n * v.n, m.d * v.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s kg m/s", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_newton2(const char *const *args, char *out, size_t outlen)
{
	rat m, a;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &m, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &a, NULL)) != 0)
		return rc;
	if ((rc = dec4_render(m.n * a.n, m.d * a.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s N", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_circular_accel(const char *const *args, char *out,
                                size_t outlen)
{
	rat v, r;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &v, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &r, NULL)) != 0)
		return rc;
	if (r.n == 0)
		return -4;
	if ((rc = dec4_render(v.n * v.n * r.d, v.d * v.d * r.n,
	                      f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s m/s^2", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_wave_speed(const char *const *args, char *out, size_t outlen)
{
	rat f, lam;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &f, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &lam, NULL)) != 0)
		return rc;
	if ((rc = dec4_render(f.n * lam.n, f.d * lam.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s m/s", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_specific_heat(const char *const *args, char *out,
                               size_t outlen)
{
	rat m, c, dT;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &m, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &c, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &dT, NULL)) != 0)
		return rc;
	if ((rc = dec4_render(m.n * c.n * dT.n, m.d * c.d * dT.d,
	                      f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s J", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_ohm_v(const char *const *args, char *out, size_t outlen)
{
	rat I, R;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &I, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &R, NULL)) != 0)
		return rc;
	if ((rc = dec4_render(I.n * R.n, I.d * R.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s V", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_ohm_power(const char *const *args, char *out, size_t outlen)
{
	rat I, V;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &I, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &V, NULL)) != 0)
		return rc;
	if ((rc = dec4_render(I.n * V.n, I.d * V.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s W", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_free_fall(const char *const *args, char *out, size_t outlen)
{
	rat h, g, s;
	int rc;
	long long s12;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &h, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &g, NULL)) != 0)
		return rc;
	s.n = 2 * h.n * g.n;
	s.d = h.d * g.d;
	if ((rc = rat_norm(&s)) != 0)
		return rc;
	if (s.n < 0)
		return -4;
	s12 = sqrt12(s.n, s.d, &rc);
	if (rc != 0)
		return rc;
	if ((rc = dec4_render(s12, 1000000000000LL, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s m/s", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_molarity(const char *const *args, char *out, size_t outlen)
{
	rat n, V;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &n, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &V, NULL)) != 0)
		return rc;
	if (V.n == 0)
		return -4;
	if ((rc = dec4_render(n.n * V.d, n.d * V.n, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s mol/L", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_ph(const char *const *args, char *out, size_t outlen)
{
	rat H;
	int rc;
	long long l;
	char f1[48];

	if ((rc = parse_rat(args[0], &H, NULL)) != 0)
		return rc;
	if (H.n <= 0)
		return -4;
	l = log10_9(H.n, H.d, &rc);
	if (rc != 0)
		return rc;
	if ((rc = dec4_render(-l, 1000000000LL, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

static int solve_poh(const char *const *args, char *out, size_t outlen)
{
	return solve_ph(args, out, outlen);
}

static int solve_ph_from_poh(const char *const *args, char *out,
                             size_t outlen)
{
	rat poh;
	int rc;
	char f1[48];

	if ((rc = parse_rat(args[0], &poh, NULL)) != 0)
		return rc;
	if ((rc = dec4_render(14 * poh.d - poh.n, poh.d, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

static int solve_percent_yield(const char *const *args, char *out,
                               size_t outlen)
{
	rat actual, theoretical;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &actual, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &theoretical, NULL)) != 0)
		return rc;
	if (theoretical.n == 0)
		return -4;
	if ((rc = dec4_render(100 * actual.n * theoretical.d,
	                      actual.d * theoretical.n, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s%%", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_boyles(const char *const *args, char *out, size_t outlen)
{
	rat P1, V1, V2;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &P1, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &V1, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &V2, NULL)) != 0)
		return rc;
	if (V2.n == 0)
		return -4;
	if ((rc = dec4_render(P1.n * V1.n * V2.d,
	                      P1.d * V1.d * V2.n, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s atm", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_charles(const char *const *args, char *out, size_t outlen)
{
	rat V1, T1, T2;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &V1, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &T1, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &T2, NULL)) != 0)
		return rc;
	if (T1.n == 0)
		return -4;
	if ((rc = dec4_render(V1.n * T2.n * T1.d,
	                      V1.d * T2.d * T1.n, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s L", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_combined_gas(const char *const *args, char *out,
                              size_t outlen)
{
	rat P1, V1, T1, P2, T2;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &P1, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &V1, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &T1, NULL)) != 0 ||
	    (rc = parse_rat(args[3], &P2, NULL)) != 0 ||
	    (rc = parse_rat(args[4], &T2, NULL)) != 0)
		return rc;
	if (T1.n == 0 || P2.n == 0)
		return -4;
	if ((rc = dec4_render(P1.n * V1.n * T2.n * T1.d * P2.d,
	                      P1.d * V1.d * T2.d * T1.n * P2.n,
	                      f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s L", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_density(const char *const *args, char *out, size_t outlen)
{
	rat m, V;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &m, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &V, NULL)) != 0)
		return rc;
	if (V.n == 0)
		return -4;
	if ((rc = dec4_render(m.n * V.d, m.d * V.n, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s g/mL", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_stoichiometry(const char *const *args, char *out,
                               size_t outlen)
{
	rat n, ratio;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &n, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &ratio, NULL)) != 0)
		return rc;
	if ((rc = dec4_render(n.n * ratio.n, n.d * ratio.d,
	                      f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s mol", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_km_to_m(const char *const *args, char *out, size_t outlen)
{
	rat km;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &km, NULL)) != 0)
		return rc;
	if ((rc = dec4_render(km.n * 1000, km.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s m", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_m_to_cm(const char *const *args, char *out, size_t outlen)
{
	rat m;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &m, NULL)) != 0)
		return rc;
	if ((rc = dec4_render(m.n * 100, m.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s cm", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_celsius_to_fahrenheit(const char *const *args, char *out,
                                       size_t outlen)
{
	rat c;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &c, NULL)) != 0)
		return rc;
	if ((rc = dec4_render(9 * c.n + 32 * 5 * c.d, 5 * c.d,
	                      f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s F", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_fahrenheit_to_celsius(const char *const *args, char *out,
                                       size_t outlen)
{
	rat f;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &f, NULL)) != 0)
		return rc;
	if ((rc = dec4_render(5 * (f.n - 32 * f.d), 9 * f.d,
	                      f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s C", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_kg_to_lb(const char *const *args, char *out, size_t outlen)
{
	rat kg;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &kg, NULL)) != 0)
		return rc;
	if ((rc = dec4_render(kg.n * 2205, kg.d * 1000, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s lb", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_liters_to_ml(const char *const *args, char *out,
                              size_t outlen)
{
	rat L;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &L, NULL)) != 0)
		return rc;
	if ((rc = dec4_render(L.n * 1000, L.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s mL", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_percent_error(const char *const *args, char *out,
                               size_t outlen)
{
	rat measured, accepted, diff;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &measured, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &accepted, NULL)) != 0)
		return rc;
	if (accepted.n == 0)
		return -4;
	if ((rc = rat_sub(&diff, &measured, &accepted)) != 0)
		return rc;
	if (diff.n < 0)
		diff.n = -diff.n;
	if ((rc = dec4_render(100 * diff.n * accepted.d,
	                      diff.d * accepted.n, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s%%", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_scientific_notation(const char *const *args, char *out,
                                     size_t outlen)
{
	rat mant, ex;
	int rc, i;
	long long num, den;
	char f1[48];

	if ((rc = parse_rat(args[0], &mant, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &ex, NULL)) != 0)
		return rc;
	if (ex.d != 1 || ex.n < -12 || ex.n > 12)
		return -4;
	num = mant.n;
	den = mant.d;
	if (ex.n >= 0) {
		for (i = 0; i < ex.n; i++) {
			if (num > 9000000000000000LL / 10)
				return -4;
			num *= 10;
		}
	} else {
		for (i = 0; i < -ex.n; i++) {
			if (den > 9000000000000000LL / 10)
				return -4;
			den *= 10;
		}
	}
	if ((rc = dec4_render(num, den, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

static int solve_average(const char *const *args, char *out, size_t outlen)
{
	rat a, b, c, d, s, t;
	int rc;
	char f1[48];

	if ((rc = parse_rat(args[0], &a, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &b, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &c, NULL)) != 0 ||
	    (rc = parse_rat(args[3], &d, NULL)) != 0)
		return rc;
	if ((rc = rat_add(&s, &a, &b)) != 0 ||
	    (rc = rat_add(&t, &c, &d)) != 0 ||
	    (rc = rat_add(&s, &s, &t)) != 0)
		return rc;
	if ((rc = dec4_render(s.n, 4 * s.d, f1, sizeof(f1))) != 0)
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

/*
 * n significant digits of the exact rational num/den (den > 0), round half
 * to even on the exact value, written as a plain decimal. Zeros that carry
 * magnitude are kept (12345 to 3 is 12300); zeros after the decimal point
 * are dropped (3.14159 to 4 is 3.142).
 */
static int sig_render(long long num, long long den, int sig, char *out,
                      size_t outlen)
{
	char digits[24], buf[48], *w;
	long long un, a, b, q, rem, t;
	size_t g, nd;
	int neg = 0, e, p, k, i;

	if (den <= 0 || sig < 1 || sig > 9)
		return -4;
	if (num == 0) {
		out_set(out, outlen, "0");
		return 0;
	}
	if (num < 0) {
		neg = 1;
		un = -num;
	} else {
		un = num;
	}
	g = (size_t)gcd_ll(un, den);
	un /= (long long)g;
	den /= (long long)g;

	if (un >= den) {
		q = un / den;
		e = 0;
		while (q >= 10) {
			q /= 10;
			e++;
		}
	} else {
		t = un;
		e = 0;
		while (t < den) {
			if (t > WIDE_LIMIT / 10)
				return -4;
			t *= 10;
			e--;
		}
	}
	p = e - sig + 1;
	if (p >= 0) {
		if (p > 18 || den > WIDE_LIMIT / P10[p])
			return -4;
		a = un;
		b = den * P10[p];
	} else {
		if (-p > 18 || un > WIDE_LIMIT / P10[-p])
			return -4;
		a = un * P10[-p];
		b = den;
	}
	q = a / b;
	rem = a % b;
	if (2 * rem > b || (2 * rem == b && (q & 1)))
		q++;

	if (p >= 0) {
		if (p > 0) {
			if (q > WIDE_LIMIT / P10[p])
				return -4;
			q *= P10[p];
		}
		snprintf(buf, sizeof(buf), "%lld", q);
	} else {
		k = -p;
		snprintf(digits, sizeof(digits), "%lld", q);
		nd = strlen(digits);
		w = buf;
		if (nd <= (size_t)k) {
			*w++ = '0';
			*w++ = '.';
			for (i = 0; i < k - (int)nd; i++)
				*w++ = '0';
			for (i = 0; i < (int)nd; i++)
				*w++ = digits[i];
		} else {
			for (i = 0; i < (int)(nd - (size_t)k); i++)
				*w++ = digits[i];
			*w++ = '.';
			for (i = (int)(nd - (size_t)k); i < (int)nd; i++)
				*w++ = digits[i];
		}
		*w = '\0';
		w = buf + strlen(buf);
		while (w > buf && w[-1] == '0')
			*--w = '\0';
		if (w > buf && w[-1] == '.')
			*--w = '\0';
	}
	if (neg) {
		w = digits;
		*w++ = '-';
		for (i = 0; buf[i] != '\0'; i++)
			*w++ = buf[i];
		*w = '\0';
		out_set(out, outlen, digits);
	} else {
		out_set(out, outlen, buf);
	}
	return 0;
}

static int solve_sig_figs(const char *const *args, char *out, size_t outlen)
{
	rat v, n;
	int rc;

	if ((rc = parse_rat(args[0], &v, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &n, NULL)) != 0)
		return rc;
	if (n.d != 1 || n.n < 1 || n.n > 9)
		return -4;
	return sig_render(v.n, v.d, (int)n.n, out, outlen);
}


int solvers_solve(const char *name, const char *const *args, int nargs,
                  char *out, size_t outlen)
{
	int rc = 0;

	if (name == NULL || args == NULL) {
		rc = -1;
	} else if (strcmp(name, "quad_solve") == 0) {
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
	} else if (strcmp(name, "kinematic_v") == 0) {
		if (nargs == 3)
			rc = solve_kinematic(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "proj_range") == 0) {
		if (nargs == 3)
			rc = solve_proj(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "ohm_i") == 0) {
		if (nargs == 2)
			rc = solve_ohm(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "moles") == 0) {
		if (nargs == 2)
			rc = solve_moles(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "dilution") == 0) {
		if (nargs == 3)
			rc = solve_dilution(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "gas_p") == 0) {
		if (nargs == 3)
			rc = solve_gasp(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "sin_eval") == 0) {
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
	} else if (strcmp(name, "kinetic_energy") == 0) {
		if (nargs == 2)
			rc = solve_kinetic_energy(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "potential_energy") == 0) {
		if (nargs == 3)
			rc = solve_potential_energy(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "work") == 0) {
		if (nargs == 3)
			rc = solve_work(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "power") == 0) {
		if (nargs == 2)
			rc = solve_power(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "momentum") == 0) {
		if (nargs == 2)
			rc = solve_momentum(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "newton2") == 0) {
		if (nargs == 2)
			rc = solve_newton2(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "circular_accel") == 0) {
		if (nargs == 2)
			rc = solve_circular_accel(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "wave_speed") == 0) {
		if (nargs == 2)
			rc = solve_wave_speed(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "specific_heat") == 0) {
		if (nargs == 3)
			rc = solve_specific_heat(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "ohm_v") == 0) {
		if (nargs == 2)
			rc = solve_ohm_v(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "ohm_power") == 0) {
		if (nargs == 2)
			rc = solve_ohm_power(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "free_fall") == 0) {
		if (nargs == 2)
			rc = solve_free_fall(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "molarity") == 0) {
		if (nargs == 2)
			rc = solve_molarity(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "ph") == 0) {
		if (nargs == 1)
			rc = solve_ph(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "poh") == 0) {
		if (nargs == 1)
			rc = solve_poh(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "ph_from_poh") == 0) {
		if (nargs == 1)
			rc = solve_ph_from_poh(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "percent_yield") == 0) {
		if (nargs == 2)
			rc = solve_percent_yield(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "boyles") == 0) {
		if (nargs == 3)
			rc = solve_boyles(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "charles") == 0) {
		if (nargs == 3)
			rc = solve_charles(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "combined_gas") == 0) {
		if (nargs == 5)
			rc = solve_combined_gas(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "density") == 0) {
		if (nargs == 2)
			rc = solve_density(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "stoichiometry") == 0) {
		if (nargs == 2)
			rc = solve_stoichiometry(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "km_to_m") == 0) {
		if (nargs == 1)
			rc = solve_km_to_m(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "m_to_cm") == 0) {
		if (nargs == 1)
			rc = solve_m_to_cm(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "celsius_to_fahrenheit") == 0) {
		if (nargs == 1)
			rc = solve_celsius_to_fahrenheit(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "fahrenheit_to_celsius") == 0) {
		if (nargs == 1)
			rc = solve_fahrenheit_to_celsius(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "kg_to_lb") == 0) {
		if (nargs == 1)
			rc = solve_kg_to_lb(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "liters_to_ml") == 0) {
		if (nargs == 1)
			rc = solve_liters_to_ml(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "percent_error") == 0) {
		if (nargs == 2)
			rc = solve_percent_error(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "scientific_notation") == 0) {
		if (nargs == 2)
			rc = solve_scientific_notation(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "average") == 0) {
		if (nargs == 4)
			rc = solve_average(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "sig_figs") == 0) {
		if (nargs == 2)
			rc = solve_sig_figs(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "sinusoid") == 0) {
		if (nargs == 3)
			rc = solve_sinusoid(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "cubic_solve") == 0) {
		if (nargs == 4)
			rc = solve_cubic_solve(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "right_triangle") == 0) {
		if (nargs == 2)
			rc = solve_right_triangle(args, out, outlen);
		else
			rc = -2;
	} else {
		rc = -1;
	}
	if (rc != 0)
		out_clear(out, outlen);
	return rc;
}
