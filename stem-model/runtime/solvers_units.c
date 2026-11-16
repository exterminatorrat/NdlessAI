#include <stdio.h>
#include <string.h>

#include "solvers.h"
#include "solvers_subject.h"

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

static int solve_percent_change(const char *const *args, char *out,
                                size_t outlen)
{
	rat o, n;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &o, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &n, NULL)) != 0)
		return rc;
	if (o.n == 0)
		return -4;
	if ((rc = dec4_render(100 * (n.n * o.d - o.n * n.d),
	                      n.d * o.n, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s%%", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_percent_of(const char *const *args, char *out, size_t outlen)
{
	rat p, w;
	int rc;
	char f1[48];

	if ((rc = parse_rat(args[0], &p, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &w, NULL)) != 0)
		return rc;
	if ((rc = dec4_render(p.n * w.n, p.d * w.d * 100, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

static int solve_ratio_share(const char *const *args, char *out, size_t outlen)
{
	rat t, a, b;
	int rc;
	char f1[48];
	long long bc, ad;

	if ((rc = parse_rat(args[0], &t, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &a, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &b, NULL)) != 0)
		return rc;
	bc = b.n * a.d;
	ad = a.n * b.d;
	if (bc + ad == 0)
		return -4;
	if ((rc = dec4_render(t.n * a.n * b.d, t.d * (bc + ad),
	                      f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

static int solve_average_speed(const char *const *args, char *out,
                               size_t outlen)
{
	rat d, t;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &d, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &t, NULL)) != 0)
		return rc;
	if (t.n == 0)
		return -4;
	if ((rc = dec4_render(d.n * t.d, d.d * t.n, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s km/h", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_simple_interest(const char *const *args, char *out,
                                 size_t outlen)
{
	rat p, r, y;
	int rc;
	char f1[48];

	if ((rc = parse_rat(args[0], &p, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &r, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &y, NULL)) != 0)
		return rc;
	if ((rc = dec4_render(p.n * r.n * y.n,
	                      p.d * r.d * y.d * 100, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

static int solve_rectangle_area(const char *const *args, char *out,
                                 size_t outlen)
{
	rat w, h;
	int rc;
	char f1[48];

	if ((rc = parse_rat(args[0], &w, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &h, NULL)) != 0)
		return rc;
	if ((rc = dec4_render(w.n * h.n, w.d * h.d, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

static int solve_triangle_area(const char *const *args, char *out,
                               size_t outlen)
{
	rat b, h;
	int rc;
	char f1[48];

	if ((rc = parse_rat(args[0], &b, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &h, NULL)) != 0)
		return rc;
	if ((rc = dec4_render(b.n * h.n, b.d * h.d * 2, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

static int solve_volume_prism(const char *const *args, char *out, size_t outlen)
{
	rat l, w, h;
	int rc;
	char f1[48];

	if ((rc = parse_rat(args[0], &l, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &w, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &h, NULL)) != 0)
		return rc;
	if ((rc = dec4_render(l.n * w.n * h.n, l.d * w.d * h.d,
	                      f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

static int solve_combined_rate(const char *const *args, char *out, size_t outlen)
{
	rat a, b;
	int rc;
	char f1[48];
	long long cross;

	if ((rc = parse_rat(args[0], &a, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &b, NULL)) != 0)
		return rc;
	cross = a.n * b.d + b.n * a.d;
	if (cross == 0)
		return -4;
	if ((rc = dec4_render(a.n * b.n, cross, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

static int solve_probability_single(const char *const *args, char *out,
                                    size_t outlen)
{
	rat f, t;
	int rc;
	char f1[48];

	if ((rc = parse_rat(args[0], &f, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &t, NULL)) != 0)
		return rc;
	if (t.n == 0)
		return -4;
	if ((rc = dec4_render(f.n * t.d, f.d * t.n, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

int solvers_units_solve(const char *name, const char *const *args,
                        int nargs, char *out, size_t outlen)
{
	int rc;

	if (strcmp(name, "km_to_m") == 0) {
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

	} else if (strcmp(name, "percent_change") == 0) {
		if (nargs == 2)
			rc = solve_percent_change(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "percent_of") == 0) {
		if (nargs == 2)
			rc = solve_percent_of(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "ratio_share") == 0) {
		if (nargs == 3)
			rc = solve_ratio_share(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "average_speed") == 0) {
		if (nargs == 2)
			rc = solve_average_speed(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "simple_interest") == 0) {
		if (nargs == 3)
			rc = solve_simple_interest(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "rectangle_area") == 0) {
		if (nargs == 2)
			rc = solve_rectangle_area(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "triangle_area") == 0) {
		if (nargs == 2)
			rc = solve_triangle_area(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "volume_prism") == 0) {
		if (nargs == 3)
			rc = solve_volume_prism(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "combined_rate") == 0) {
		if (nargs == 2)
			rc = solve_combined_rate(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "probability_single") == 0) {
		if (nargs == 2)
			rc = solve_probability_single(args, out, outlen);
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
	} else {
		return 0;
	}
	return rc == 0 ? 1 : rc;
}
