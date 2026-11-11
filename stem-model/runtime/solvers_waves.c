#include <stdio.h>
#include <string.h>

#include "solvers.h"
#include "solvers_subject.h"

/*
 * Waves and sound: the seven relations AP Physics 2 states in decibels,
 * hertz and millimetres.
 *
 * Four are exact rationals of the decimal arguments and render through
 * dec4_render. The rest need the fixed-point logarithm, exponential or square
 * root, and those carry explicit range checks: the helpers refuse what is
 * outside their own domain, but the scale factors this file multiplies by can
 * wrap a long long before the helper ever sees the value.
 */

static int solve_sound_level(const char *const *args, char *out, size_t outlen)
{
	rat i;
	int rc;
	long long l9, num;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &i, NULL)) != 0)
		return rc;
	if (i.n <= 0)
		return -4;
	/*
	 * L = 10*log10(I/1e-12) = 10*(log10(I) + 12). The 12 is added in the
	 * 1e9-scaled log domain instead of scaling I first: I*1e12 as a rational
	 * leaves the 1e6 ratio log_core_9 accepts for every intensity above
	 * 1e-6 W/m^2, which is all of the audible range.
	 */
	l9 = log10_9(i.n, i.d, &rc);
	if (rc != 0)
		return rc;
	num = 10 * (l9 + 12000000000LL);
	if (num > WIDE_LIMIT || -num > WIDE_LIMIT)
		return -4;
	if ((rc = dec4_render(num, 1000000000LL, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s dB", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_sound_intensity(const char *const *args, char *out,
                                 size_t outlen)
{
	rat l;
	int rc;
	long long ln10_9, dn, dd, x9, i9;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &l, NULL)) != 0)
		return rc;
	/*
	 * I = 1e-12*10^(L/10) = exp(ln(10)*(L - 120)/10). The shift by 120 dB
	 * turns the 1e-12 into an exponent instead of a factor, which keeps the
	 * argument inside exp_9 and the intensity itself a plain decimal.
	 */
	dn = l.n - 120LL * l.d;
	dd = l.d;
	if (dn > 9000LL || -dn > 9000LL)
		return -4;
	ln10_9 = ln_9(10, 1, &rc);
	if (rc != 0)
		return rc;
	x9 = div_round(ln10_9 * dn, 10 * dd);
	i9 = exp_9(x9, &rc);
	if (rc != 0)
		return rc;
	if ((rc = dec4_render(i9, 1000000000LL, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s W/m^2", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_doppler_freq(const char *const *args, char *out,
                              size_t outlen)
{
	rat f, v, vs, gap, fp;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &f, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &v, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &vs, NULL)) != 0)
		return rc;
	if ((rc = rat_sub(&gap, &v, &vs)) != 0)
		return rc;
	if (gap.n == 0)
		return -4;
	if ((rc = rat_mul(&fp, &f, &v)) != 0 ||
	    (rc = rat_div(&fp, &fp, &gap)) != 0)
		return rc;
	if ((rc = dec4_render(fp.n, fp.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s Hz", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_beat_freq(const char *const *args, char *out, size_t outlen)
{
	rat f1, f2, fb;
	int rc;
	char num[48], buf[64];

	if ((rc = parse_rat(args[0], &f1, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &f2, NULL)) != 0)
		return rc;
	if ((rc = rat_sub(&fb, &f1, &f2)) != 0)
		return rc;
	if (fb.n < 0)
		fb.n = -fb.n;
	if ((rc = dec4_render(fb.n, fb.d, num, sizeof(num))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s Hz", num);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_standing_wave_freq(const char *const *args, char *out,
                                    size_t outlen)
{
	rat n, v, l, twice_l, fn;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &n, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &v, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &l, NULL)) != 0)
		return rc;
	if ((rc = rat_mul(&fn, &n, &v)) != 0)
		return rc;
	twice_l.n = 2 * l.n;
	twice_l.d = l.d;
	if ((rc = rat_div(&fn, &fn, &twice_l)) != 0)
		return rc;
	if ((rc = dec4_render(fn.n, fn.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s Hz", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_string_freq(const char *const *args, char *out,
                             size_t outlen)
{
	rat t, mu, l, ratio;
	int rc;
	long long s12, q9, num, den;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &t, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &mu, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &l, NULL)) != 0)
		return rc;
	if (t.n <= 0 || mu.n <= 0 || l.n == 0)
		return -4;
	if (l.n > 9000LL || -l.n > 9000LL || l.d > 1000LL)
		return -4;
	ratio.n = t.n * mu.d;
	ratio.d = t.d * mu.n;
	if ((rc = rat_norm(&ratio)) != 0)
		return rc;
	s12 = sqrt12(ratio.n, ratio.d, &rc);
	if (rc != 0)
		return rc;
	/*
	 * sqrt12 answers at 1e12 and the wavelength sweep only needs nine
	 * digits, so drop to 1e9 first: that keeps s12*L.d under dec4_render's
	 * numerator limit for every argument the relation accepts.
	 */
	q9 = div_round(s12, 1000);
	num = q9 * l.d;
	den = 2000000000LL * l.n;
	if ((rc = dec4_render(num, den, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s Hz", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_double_slit_fringe(const char *const *args, char *out,
                                    size_t outlen)
{
	rat m, lam, l, d, thousand, y;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &m, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &lam, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &l, NULL)) != 0 ||
	    (rc = parse_rat(args[3], &d, NULL)) != 0)
		return rc;
	thousand.n = 1000;
	thousand.d = 1;
	if ((rc = rat_mul(&y, &m, &lam)) != 0 ||
	    (rc = rat_mul(&y, &y, &l)) != 0 ||
	    (rc = rat_mul(&d, &d, &thousand)) != 0)
		return rc;
	if ((rc = rat_div(&y, &y, &d)) != 0)
		return rc;
	if ((rc = dec4_render(y.n, y.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s mm", f1);
	out_set(out, outlen, buf);
	return 0;
}

int solvers_waves_solve(const char *name, const char *const *args,
                        int nargs, char *out, size_t outlen)
{
	int rc;

	if (strcmp(name, "sound_level") == 0) {
		if (nargs == 1)
			rc = solve_sound_level(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "sound_intensity") == 0) {
		if (nargs == 1)
			rc = solve_sound_intensity(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "doppler_freq") == 0) {
		if (nargs == 3)
			rc = solve_doppler_freq(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "beat_freq") == 0) {
		if (nargs == 2)
			rc = solve_beat_freq(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "standing_wave_freq") == 0) {
		if (nargs == 3)
			rc = solve_standing_wave_freq(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "string_freq") == 0) {
		if (nargs == 3)
			rc = solve_string_freq(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "double_slit_fringe") == 0) {
		if (nargs == 4)
			rc = solve_double_slit_fringe(args, out, outlen);
		else
			rc = -2;
	} else {
		return 0;
	}
	return rc == 0 ? 1 : rc;
}
