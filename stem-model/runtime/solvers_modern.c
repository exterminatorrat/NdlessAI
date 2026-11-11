#include <stdio.h>
#include <string.h>

#include "solvers.h"
#include "solvers_subject.h"

/*
 * Modern physics in the units the course uses: photon energies in eV with the
 * wavelength in nm (h*c = 1240 eV nm), masses in atomic mass units
 * (1 u = 931.5 MeV), activity in becquerel.
 *
 * The choice is forced as well as conventional. In SI the numerator would be
 * h*c = 1.986e-25, which parse_rat cannot represent at all, so every relation
 * here is written against the exam's own constant.
 *
 * Four of the nine are exact rationals of the typed numbers and render through
 * dec4_render. The other two cannot be: a de Broglie wavelength needs a square
 * root and a surviving fraction needs 0.5 raised to a rational power, so those
 * go through sqrt12 and ln_9/exp_9 at 1e9 or 1e12 scale.
 */

static int solve_photon_energy_ev(const char *const *args, char *out,
                                  size_t outlen)
{
	rat lam;
	long long num, den;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &lam, NULL)) != 0)
		return rc;
	if (lam.n == 0)
		return -4;
	num = 1240 * lam.d;
	den = lam.n;
	if (den < 0) {
		den = -den;
		num = -num;
	}
	if ((rc = dec4_render(num, den, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s eV", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_photoelectric_kev(const char *const *args, char *out,
                                   size_t outlen)
{
	rat lam, phi, e, k;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &lam, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &phi, NULL)) != 0)
		return rc;
	e.n = 1240;
	e.d = 1;
	if ((rc = rat_div(&k, &e, &lam)) != 0)
		return rc;
	if ((rc = rat_sub(&e, &k, &phi)) != 0)
		return rc;
	if ((rc = dec4_render(e.n, e.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s eV", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_threshold_wavelength(const char *const *args, char *out,
                                      size_t outlen)
{
	rat phi;
	long long num, den;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &phi, NULL)) != 0)
		return rc;
	if (phi.n == 0)
		return -4;
	num = 1240 * phi.d;
	den = phi.n;
	if (den < 0) {
		den = -den;
		num = -num;
	}
	if ((rc = dec4_render(num, den, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s nm", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_de_broglie_electron(const char *const *args, char *out,
                                     size_t outlen)
{
	const long long HC_NM = 1226000000000LL;
	rat v;
	long long root12;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &v, NULL)) != 0)
		return rc;
	if (v.n <= 0)
		return -4;
	root12 = sqrt12(v.n, v.d, &rc);
	if (rc != 0)
		return rc;
	/*
	 * sqrt12 answers sqrt(V) at 1e12, so 1.226/sqrt(V) is 1.226e12 divided
	 * by that root: both sides stay integers and no constant is rounded
	 * before the final four-digit render.
	 */
	if ((rc = dec4_render(HC_NM, root12, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s nm", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_bohr_energy_ev(const char *const *args, char *out,
                                size_t outlen)
{
	rat n;
	long long num, den;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &n, NULL)) != 0)
		return rc;
	if (n.n == 0)
		return -4;
	/*
	 * E = -13.6/n^2 with n reduced: the 13.6 is the exact fraction 136/10,
	 * and the square of n has to be taken over its numerator and its
	 * denominator separately so 1.5 stays exact as well.
	 */
	if (n.n > 100000 || -n.n > 100000 || n.d > 100000)
		return -4;
	num = -136 * n.d * n.d;
	den = 10 * n.n * n.n;
	if ((rc = dec4_render(num, den, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s eV", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_bohr_transition_nm(const char *const *args, char *out,
                                    size_t outlen)
{
	rat n1, n2;
	long long a, b, t, num, den;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &n1, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &n2, NULL)) != 0)
		return rc;
	if (n1.n == 0 || n2.n == 0)
		return -4;
	if (n1.n > 100000 || -n1.n > 100000 || n1.d > 1000 ||
	    n2.n > 100000 || -n2.n > 100000 || n2.d > 1000)
		return -4;
	/*
	 * 1240/(13.6*(1/n1^2 - 1/n2^2)) collapses to
	 * 1550*n1^2*n2^2 / (17*(n2^2 - n1^2)) once 13.6 is written as 136/10
	 * and the compound fraction is cleared. Guarded before the products
	 * rather than after: n^2 is squared again and would otherwise wrap.
	 */
	a = n1.n * n1.n;
	b = n2.n * n2.n;
	if (a != 0 && b > (WIDE_LIMIT / 1550) / a)
		return -4;
	num = 1550 * a * b;
	t = b * n1.d * n1.d - a * n2.d * n2.d;
	if (t == 0)
		return -4;
	den = 17 * t;
	if (den < 0) {
		den = -den;
		num = -num;
	}
	if ((rc = dec4_render(num, den, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s nm", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_mass_energy_mev(const char *const *args, char *out,
                                 size_t outlen)
{
	rat m;
	long long num, den;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &m, NULL)) != 0)
		return rc;
	if (m.n > 1000000 || -m.n > 1000000)
		return -4;
	num = 1863 * m.n;
	den = 2 * m.d;
	if ((rc = dec4_render(num, den, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s MeV", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_half_life_fraction(const char *const *args, char *out,
                                    size_t outlen)
{
	rat t, th;
	long long l9, num, den, x9, e9, k, pow;
	int i, rc;
	char f1[48];

	if ((rc = parse_rat(args[0], &t, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &th, NULL)) != 0)
		return rc;
	if (th.n == 0)
		return -4;
	if (t.n > 10000 || -t.n > 10000 || t.d > 10000 ||
	    th.n > 10000 || -th.n > 10000 || th.d > 10000)
		return -4;
	/*
	 * A whole number of half lives is exactly 1/2^k, and the six-half-life
	 * case is exactly 0.015625 - a four-digit tie. Routing that through the
	 * log and exponential series would let the series' own error choose a
	 * side of the tie, so the exact power is rendered directly instead.
	 */
	num = t.n * th.d;
	den = t.d * th.n;
	if (den > 0 && num >= 0 && num % den == 0) {
		k = num / den;
		if (k > 59)
			return -4;
		pow = 1;
		for (i = 0; i < k; i++)
			pow *= 2;
		if ((rc = dec4_render(1, pow, f1, sizeof(f1))) != 0)
			return rc;
		out_set(out, outlen, f1);
		return 0;
	}
	l9 = ln_9(1, 2, &rc);
	if (rc != 0)
		return rc;
	/*
	 * 0.5^x = exp(ln(0.5)*x). ln_9 is at 1e9 and exp_9 takes its argument
	 * at 1e9, so the only work is forming x = t/t_half in that scale;
	 * div_round keeps the argument itself unrounded enough that four
	 * digits of the result survive.
	 */
	num = l9 * t.n * th.d;
	den = t.d * th.n;
	if (den < 0) {
		den = -den;
		num = -num;
	}
	x9 = div_round(num, den);
	e9 = exp_9(x9, &rc);
	if (rc != 0)
		return rc;
	if ((rc = dec4_render(e9, 1000000000LL, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

static int solve_activity_bq(const char *const *args, char *out,
                             size_t outlen)
{
	rat n, th;
	long long num, den;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &n, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &th, NULL)) != 0)
		return rc;
	if (th.n == 0)
		return -4;
	/*
	 * A = 0.693*N/t_half with N up to the billions: the product is built
	 * directly instead of through rat_mul, which refuses numerators above
	 * 2e9, and the bounds keep 693*N inside the renderer's range.
	 */
	if (n.n > 10000000000LL || -n.n > 10000000000LL || n.d > 1000 ||
	    th.n > 1000000 || -th.n > 1000000 || th.d > 1000)
		return -4;
	num = 693 * n.n * th.d;
	den = 1000 * n.d * th.n;
	if (den < 0) {
		den = -den;
		num = -num;
	}
	if ((rc = dec4_render(num, den, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s Bq", f1);
	out_set(out, outlen, buf);
	return 0;
}

int solvers_modern_solve(const char *name, const char *const *args,
                         int nargs, char *out, size_t outlen)
{
	int rc;

	if (strcmp(name, "photon_energy_ev") == 0) {
		if (nargs == 1)
			rc = solve_photon_energy_ev(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "photoelectric_kev") == 0) {
		if (nargs == 2)
			rc = solve_photoelectric_kev(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "threshold_wavelength") == 0) {
		if (nargs == 1)
			rc = solve_threshold_wavelength(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "de_broglie_electron") == 0) {
		if (nargs == 1)
			rc = solve_de_broglie_electron(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "bohr_energy_ev") == 0) {
		if (nargs == 1)
			rc = solve_bohr_energy_ev(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "bohr_transition_nm") == 0) {
		if (nargs == 2)
			rc = solve_bohr_transition_nm(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "mass_energy_mev") == 0) {
		if (nargs == 1)
			rc = solve_mass_energy_mev(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "half_life_fraction") == 0) {
		if (nargs == 2)
			rc = solve_half_life_fraction(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "activity_bq") == 0) {
		if (nargs == 2)
			rc = solve_activity_bq(args, out, outlen);
		else
			rc = -2;
	} else {
		return 0;
	}
	return rc == 0 ? 1 : rc;
}
