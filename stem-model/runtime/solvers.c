#include <stdio.h>
#include <string.h>

#include "solvers.h"
#include "solvers_subject.h"

#define OP_LIMIT    2000000000LL
#define DEC_DEN_MAX 900000000000000000LL

const long long P10[] = {
	1LL, 10LL, 100LL, 1000LL, 10000LL, 100000LL, 1000000LL,
	10000000LL, 100000000LL, 1000000000LL, 10000000000LL,
	100000000000LL, 1000000000000LL, 10000000000000LL,
	100000000000000LL, 1000000000000000LL, 10000000000000000LL,
	100000000000000000LL, 1000000000000000000LL
};

long long gcd_ll(long long a, long long b)
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

int rat_norm(rat *r)
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

int rat_add(rat *r, const rat *a, const rat *b)
{
	if (!rat_small(a) || !rat_small(b))
		return -4;
	r->n = a->n * b->d + b->n * a->d;
	r->d = a->d * b->d;
	return rat_norm(r);
}

int rat_sub(rat *r, const rat *a, const rat *b)
{
	if (!rat_small(a) || !rat_small(b))
		return -4;
	r->n = a->n * b->d - b->n * a->d;
	r->d = a->d * b->d;
	return rat_norm(r);
}

int rat_mul(rat *r, const rat *a, const rat *b)
{
	if (!rat_small(a) || !rat_small(b))
		return -4;
	r->n = a->n * b->n;
	r->d = a->d * b->d;
	return rat_norm(r);
}

int rat_div(rat *r, const rat *a, const rat *b)
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

unsigned long long isqrt64(unsigned long long n)
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

long long div_round(long long a, long long b)
{
	if (a >= 0)
		return (a + b / 2) / b;
	return -((-a + b / 2) / b);
}

int pow10_index(long long d)
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

void out_set(char *out, size_t outlen, const char *buf)
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
int dec4_render(long long num, long long den, char *out, size_t outlen)
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

void fmt_rat(const rat *r, char *out, size_t outlen)
{
	char buf[48];

	if (r->d == 1)
		snprintf(buf, sizeof(buf), "%lld", r->n);
	else
		snprintf(buf, sizeof(buf), "%lld/%lld", r->n, r->d);
	out_set(out, outlen, buf);
}

int parse_rat(const char *s, rat *r, int *flt)
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
long long sin9(long long tnum, int texp, int *rc)
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
long long sqrt12(long long n, long long d, int *rc)
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

/*
 * atanh((m-1)/(m+1)) at 1e9 scale, for x = num/den > 0 written as m*10^k
 * with m in [0.316, 3.163). The series converges in about 20 terms there.
 * Callers combine k and the returned sum with their own base constant.
 */
long long log_core_9(long long num, long long den, long long *k_out,
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

long long log10_9(long long num, long long den, int *rc)
{
	long long k = 0, s9 = log_core_9(num, den, &k, rc);

	if (*rc != 0)
		return 0;
	return k * 1000000000LL + s9 * 868588964LL / 1000000000LL;
}

long long ln_9(long long num, long long den, int *rc)
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
long long exp_9(long long x9, int *rc)
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


int solvers_solve(const char *name, const char *const *args, int nargs,
                  char *out, size_t outlen)
{
	static const solvers_subject_fn subject[] = {
		solvers_math_solve,
		solvers_trig_solve,
		solvers_physics_solve,
		solvers_chem_solve,
		solvers_thermo_solve,
		solvers_em_solve,
		solvers_units_solve
	};
	size_t i;
	int rc = -1;

	if (name != NULL && args != NULL) {
		for (i = 0; i < sizeof subject / sizeof subject[0]; i++) {
			rc = subject[i](name, args, nargs, out, outlen);
			if (rc != 0)
				break;
		}
		if (rc == 0)
			rc = -1;
	}
	if (rc > 0)
		rc = 0;
	if (rc != 0)
		out_clear(out, outlen);
	return rc;
}
