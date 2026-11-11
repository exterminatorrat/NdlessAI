#include <stdio.h>
#include <string.h>

#include "solvers.h"
#include "solvers_subject.h"

/*
 * Thermodynamics beyond the gas laws and calorimetry in solvers_chem.c: work at
 * constant pressure and in an adiabatic expansion, heat at constant pressure,
 * entropy, conduction, radiation, heat-pump performance, Carnot efficiency, the
 * internal energy of a monatomic gas and the gas relations that need R: density
 * and the rms molecular speed.
 *
 * Every one is an exact rational of the decimal arguments, rendered through
 * dec4_render, so the device prints the same four significant digits the
 * catalogue does. Radiation is the one that needs care: T^4 is exact, but the
 * intermediate has to stay inside the fixed-point domain, which is why the
 * Stefan-Boltzmann constant is applied last. rms_speed is the only square root;
 * its radicand reaches 6.2e6, past what sqrt12 accepts, so it takes the integer
 * root itself.
 */

static int solve_gas_work_isobaric(const char *const *args, char *out,
                                   size_t outlen)
{
	rat p, dv, w;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &p, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &dv, NULL)) != 0)
		return rc;
	if ((rc = rat_mul(&w, &p, &dv)) != 0)
		return rc;
	if ((rc = dec4_render(w.n, w.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s J", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_gas_heat_cp(const char *const *args, char *out, size_t outlen)
{
	rat n, cp, dt, q;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &n, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &cp, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &dt, NULL)) != 0)
		return rc;
	if ((rc = rat_mul(&q, &n, &cp)) != 0 ||
	    (rc = rat_mul(&q, &q, &dt)) != 0)
		return rc;
	if ((rc = dec4_render(q.n, q.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s J", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_entropy_change(const char *const *args, char *out,
                                size_t outlen)
{
	rat q, t, ds;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &q, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &t, NULL)) != 0)
		return rc;
	if (t.n == 0)
		return -4;
	if ((rc = rat_div(&ds, &q, &t)) != 0)
		return rc;
	if ((rc = dec4_render(ds.n, ds.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s J/K", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_conduction_rate(const char *const *args, char *out,
                                 size_t outlen)
{
	rat k, a, dt, d, q;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &k, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &a, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &dt, NULL)) != 0 ||
	    (rc = parse_rat(args[3], &d, NULL)) != 0)
		return rc;
	if (d.n == 0)
		return -4;
	if ((rc = rat_mul(&q, &k, &a)) != 0 ||
	    (rc = rat_mul(&q, &q, &dt)) != 0 ||
	    (rc = rat_div(&q, &q, &d)) != 0)
		return rc;
	if ((rc = dec4_render(q.n, q.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s W", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_radiation_power(const char *const *args, char *out,
                                 size_t outlen)
{
	rat t, area, p, sigma, hundred_thousand;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &t, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &area, NULL)) != 0)
		return rc;

	hundred_thousand.n = 100000;
	hundred_thousand.d = 1;
	sigma.n = 567;			/* 5.67e-8 = 567 / 1e10, applied above */
	sigma.d = 1;

	/* T^4 has to be built in scaled pieces: 400^4 is 2.56e10, and the
	 * rational helpers refuse intermediates that large. Dividing T^2 by
	 * 1e5 before squaring keeps every step under a hundred, and 1e5 * 1e5
	 * is exactly the 1e10 the constant needs, so nothing is approximated. */
	if ((rc = rat_mul(&p, &t, &t)) != 0 ||
	    (rc = rat_div(&p, &p, &hundred_thousand)) != 0 ||
	    (rc = rat_mul(&p, &p, &p)) != 0 ||
	    (rc = rat_mul(&p, &p, &area)) != 0)
		return rc;

	if ((rc = rat_mul(&p, &p, &sigma)) != 0)
		return rc;
	if ((rc = dec4_render(p.n, p.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s W", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_heat_pump_cop(const char *const *args, char *out,
                               size_t outlen)
{
	rat tc, th, d, cop;
	int rc;
	char f1[48];

	if ((rc = parse_rat(args[0], &tc, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &th, NULL)) != 0)
		return rc;
	if ((rc = rat_sub(&d, &th, &tc)) != 0)
		return rc;
	if (d.n == 0)
		return -4;
	if ((rc = rat_div(&cop, &tc, &d)) != 0)
		return rc;
	if ((rc = dec4_render(cop.n, cop.d, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

static int solve_rms_speed(const char *const *args, char *out, size_t outlen)
{
	rat t, m;
	int rc;
	long long p, q, g, r6;
	unsigned long long s6;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &t, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &m, NULL)) != 0)
		return rc;
	if (t.n <= 0 || m.n <= 0)
		return -4;

	/* v^2 = 3*R*T/M with R = 8314/1000, kept as a single rational so the
	 * square root is taken once: p/q = 3*8314*T.n*M.d / (1000*T.d*M.n). */
	p = 3 * 8314 * t.n * m.d;
	q = 1000 * t.d * m.n;
	if (p <= 0 || q <= 0 || p > 9000000000000LL)
		return -4;
	g = gcd_ll(p, q);
	p /= g;
	q /= g;

	/*
	 * sqrt12 refuses an n*d product above 4e6, which caps the radicand at
	 * 4e6; rms_speed reaches 6.2e6 for hydrogen at 500 K, so it takes the
	 * integer square root itself. Scaling v^2 by 1e6 before the root puts
	 * the answer on a 1e6 grid, which is four digits more than the
	 * renderer keeps.
	 */
	r6 = p * 1000000LL / q;
	if (r6 <= 0) {
		out_set(out, outlen, "0 m/s");
		return 0;
	}
	if (r6 > 18000000000000LL)
		return -4;
	s6 = isqrt64((unsigned long long)r6 * 1000000ULL);
	if ((rc = dec4_render((long long)s6, 1000000LL, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s m/s", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_gas_density(const char *const *args, char *out,
                             size_t outlen)
{
	rat p, m, t, r, num, den, rho;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &p, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &m, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &t, NULL)) != 0)
		return rc;
	r.n = 8314;
	r.d = 1000;
	if ((rc = rat_mul(&num, &p, &m)) != 0 ||
	    (rc = rat_mul(&den, &r, &t)) != 0)
		return rc;
	if ((rc = rat_div(&rho, &num, &den)) != 0)
		return rc;
	if ((rc = dec4_render(rho.n, rho.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s kg/m^3", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_adiabatic_work(const char *const *args, char *out,
                                size_t outlen)
{
	rat p1, v1, p2, v2, gamma, a, b, w, one;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &p1, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &v1, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &p2, NULL)) != 0 ||
	    (rc = parse_rat(args[3], &v2, NULL)) != 0 ||
	    (rc = parse_rat(args[4], &gamma, NULL)) != 0)
		return rc;
	one.n = 1;
	one.d = 1;
	if ((rc = rat_mul(&a, &p1, &v1)) != 0 ||
	    (rc = rat_mul(&b, &p2, &v2)) != 0 ||
	    (rc = rat_sub(&w, &a, &b)) != 0 ||
	    (rc = rat_sub(&gamma, &gamma, &one)) != 0)
		return rc;
	if (gamma.n == 0)
		return -4;
	if ((rc = rat_div(&w, &w, &gamma)) != 0)
		return rc;
	if ((rc = dec4_render(w.n, w.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s J", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_carnot_efficiency(const char *const *args, char *out,
                                   size_t outlen)
{
	rat tc, th, d, eta;
	int rc;
	char f1[48];

	if ((rc = parse_rat(args[0], &tc, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &th, NULL)) != 0)
		return rc;
	if (th.n == 0)
		return -4;
	if ((rc = rat_sub(&d, &th, &tc)) != 0 ||
	    (rc = rat_div(&eta, &d, &th)) != 0)
		return rc;
	if ((rc = dec4_render(eta.n, eta.d, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

static int solve_internal_energy_gas(const char *const *args, char *out,
                                     size_t outlen)
{
	rat t, three, r, two, u;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &t, NULL)) != 0)
		return rc;
	three.n = 3;
	three.d = 1;
	r.n = 8314;
	r.d = 1000;
	two.n = 2;
	two.d = 1;
	if ((rc = rat_mul(&u, &t, &three)) != 0 ||
	    (rc = rat_mul(&u, &u, &r)) != 0 ||
	    (rc = rat_div(&u, &u, &two)) != 0)
		return rc;
	if ((rc = dec4_render(u.n, u.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s J/mol", f1);
	out_set(out, outlen, buf);
	return 0;
}

int solvers_thermo_solve(const char *name, const char *const *args,
                         int nargs, char *out, size_t outlen)
{
	int rc;

	if (strcmp(name, "gas_work_isobaric") == 0) {
		if (nargs == 2)
			rc = solve_gas_work_isobaric(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "gas_heat_cp") == 0) {
		if (nargs == 3)
			rc = solve_gas_heat_cp(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "entropy_change") == 0) {
		if (nargs == 2)
			rc = solve_entropy_change(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "conduction_rate") == 0) {
		if (nargs == 4)
			rc = solve_conduction_rate(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "radiation_power") == 0) {
		if (nargs == 2)
			rc = solve_radiation_power(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "heat_pump_cop") == 0) {
		if (nargs == 2)
			rc = solve_heat_pump_cop(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "rms_speed") == 0) {
		if (nargs == 2)
			rc = solve_rms_speed(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "gas_density") == 0) {
		if (nargs == 3)
			rc = solve_gas_density(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "adiabatic_work") == 0) {
		if (nargs == 5)
			rc = solve_adiabatic_work(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "carnot_efficiency") == 0) {
		if (nargs == 2)
			rc = solve_carnot_efficiency(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "internal_energy_gas") == 0) {
		if (nargs == 1)
			rc = solve_internal_energy_gas(args, out, outlen);
		else
			rc = -2;
	} else {
		return 0;
	}
	return rc == 0 ? 1 : rc;
}
