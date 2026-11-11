#include <stdio.h>
#include <string.h>

#include "solvers.h"
#include "solvers_subject.h"

/*
 * Fluid mechanics, on the units the questions state: densities in kg/m^3,
 * lengths in m, areas in m^2, speeds in m/s and pressures in Pa.
 *
 * Every answer is an exact rational of the decimal arguments. g is an argument
 * for the hydrostatic relations, so 9.81 stays 981/100 and the four
 * significant digits come from rounding that exact rational. Bernoulli's
 * equation fixes g at 9.8, which keeps the static term rational too.
 */

static int solve_gauge_pressure(const char *const *args, char *out,
                                size_t outlen)
{
	rat rho, h, g, p;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &rho, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &h, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &g, NULL)) != 0)
		return rc;
	if ((rc = rat_mul(&p, &rho, &g)) != 0 ||
	    (rc = rat_mul(&p, &p, &h)) != 0)
		return rc;
	if ((rc = dec4_render(p.n, p.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s Pa", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_buoyant_force(const char *const *args, char *out,
                               size_t outlen)
{
	rat rho, v, g, f;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &rho, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &v, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &g, NULL)) != 0)
		return rc;
	if ((rc = rat_mul(&f, &rho, &v)) != 0 ||
	    (rc = rat_mul(&f, &f, &g)) != 0)
		return rc;
	if ((rc = dec4_render(f.n, f.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s N", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_continuity_velocity(const char *const *args, char *out,
                                     size_t outlen)
{
	rat a1, v1, a2, v2;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &a1, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &v1, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &a2, NULL)) != 0)
		return rc;
	if (a2.n == 0)
		return -4;
	if ((rc = rat_mul(&v2, &a1, &v1)) != 0 ||
	    (rc = rat_div(&v2, &v2, &a2)) != 0)
		return rc;
	if ((rc = dec4_render(v2.n, v2.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s m/s", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_bernoulli_pressure(const char *const *args, char *out,
                                    size_t outlen)
{
	rat p1, v1, v2, h1, h2, rho, d, t, p2;
	rat g, two;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &p1, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &v1, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &v2, NULL)) != 0 ||
	    (rc = parse_rat(args[3], &h1, NULL)) != 0 ||
	    (rc = parse_rat(args[4], &h2, NULL)) != 0 ||
	    (rc = parse_rat(args[5], &rho, NULL)) != 0)
		return rc;

	/* P2 = P1 + rho*(v1^2 - v2^2)/2 + rho*g*(h1 - h2), g fixed at 9.8. */
	g.n = 49;
	g.d = 5;
	two.n = 2;
	two.d = 1;
	if ((rc = rat_mul(&d, &v1, &v1)) != 0 ||
	    (rc = rat_mul(&t, &v2, &v2)) != 0 ||
	    (rc = rat_sub(&d, &d, &t)) != 0 ||
	    (rc = rat_mul(&t, &rho, &d)) != 0 ||
	    (rc = rat_div(&t, &t, &two)) != 0 ||
	    (rc = rat_add(&p2, &p1, &t)) != 0)
		return rc;
	if ((rc = rat_sub(&d, &h1, &h2)) != 0 ||
	    (rc = rat_mul(&t, &rho, &g)) != 0 ||
	    (rc = rat_mul(&t, &t, &d)) != 0 ||
	    (rc = rat_add(&p2, &p2, &t)) != 0)
		return rc;
	if ((rc = dec4_render(p2.n, p2.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s Pa", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_volume_flow_rate(const char *const *args, char *out,
                                  size_t outlen)
{
	rat a, v, q;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &a, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &v, NULL)) != 0)
		return rc;
	if ((rc = rat_mul(&q, &a, &v)) != 0)
		return rc;
	if ((rc = dec4_render(q.n, q.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s m^3/s", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_pascal_press(const char *const *args, char *out,
                              size_t outlen)
{
	rat f1rat, a1, a2, f2;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &f1rat, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &a1, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &a2, NULL)) != 0)
		return rc;
	if (a1.n == 0)
		return -4;
	if ((rc = rat_mul(&f2, &f1rat, &a2)) != 0 ||
	    (rc = rat_div(&f2, &f2, &a1)) != 0)
		return rc;
	if ((rc = dec4_render(f2.n, f2.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s N", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_submerged_fraction(const char *const *args, char *out,
                                    size_t outlen)
{
	rat ro, rf, x;
	int rc;
	char f1[48];

	if ((rc = parse_rat(args[0], &ro, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &rf, NULL)) != 0)
		return rc;
	if (rf.n == 0)
		return -4;
	if ((rc = rat_div(&x, &ro, &rf)) != 0)
		return rc;
	if ((rc = dec4_render(x.n, x.d, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

int solvers_fluids_solve(const char *name, const char *const *args,
                         int nargs, char *out, size_t outlen)
{
	int rc;

	if (strcmp(name, "gauge_pressure") == 0) {
		if (nargs == 3)
			rc = solve_gauge_pressure(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "buoyant_force") == 0) {
		if (nargs == 3)
			rc = solve_buoyant_force(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "continuity_velocity") == 0) {
		if (nargs == 3)
			rc = solve_continuity_velocity(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "bernoulli_pressure") == 0) {
		if (nargs == 6)
			rc = solve_bernoulli_pressure(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "volume_flow_rate") == 0) {
		if (nargs == 2)
			rc = solve_volume_flow_rate(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "pascal_press") == 0) {
		if (nargs == 3)
			rc = solve_pascal_press(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "submerged_fraction") == 0) {
		if (nargs == 2)
			rc = solve_submerged_fraction(args, out, outlen);
		else
			rc = -2;
	} else {
		return 0;
	}
	return rc == 0 ? 1 : rc;
}
