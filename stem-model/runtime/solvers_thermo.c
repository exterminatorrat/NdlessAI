#include <stdio.h>
#include <string.h>

#include "solvers.h"
#include "solvers_subject.h"

/*
 * Thermodynamics beyond the gas laws and calorimetry in solvers_chem.c: work at
 * constant pressure, heat at constant pressure, entropy, conduction, radiation
 * and heat-pump performance.
 *
 * Every one is an exact rational of the decimal arguments, rendered through
 * dec4_render, so the device prints the same four significant digits the
 * catalogue does. Radiation is the one that needs care: T^4 is exact, but the
 * intermediate has to stay inside the fixed-point domain, which is why the
 * Stefan-Boltzmann constant is applied last.
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
	} else {
		return 0;
	}
	return rc == 0 ? 1 : rc;
}
