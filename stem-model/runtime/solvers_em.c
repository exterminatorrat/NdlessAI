#include <stdio.h>
#include <string.h>

#include "solvers.h"
#include "solvers_subject.h"

/*
 * Electricity and magnetism, on the same unit conventions the catalogue states
 * in each question: charge in microcoulombs, capacitance in microfarads, and
 * the answers in newtons, microcoulombs, microjoules, ohms and volts.
 *
 * Coulomb's constant is folded into 0.00899 so that q1 q2 in microcoulombs
 * squared over r^2 in metres lands in newtons, and into 8990 for the field of
 * a single charge. Doing it that way keeps every intermediate inside the exact
 * fixed-point domain instead of multiplying by 8.99e9 first.
 */

static int solve_coulomb_force(const char *const *args, char *out,
                               size_t outlen)
{
	rat q1, q2, r, k, f;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &q1, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &q2, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &r, NULL)) != 0)
		return rc;
	if (r.n == 0)
		return -4;
	if ((rc = rat_mul(&f, &q1, &q2)) != 0 ||
	    (rc = rat_mul(&r, &r, &r)) != 0 ||
	    (rc = rat_div(&f, &f, &r)) != 0)
		return rc;
	k.n = 899;
	k.d = 100000;			/* 8.99e-3 N m^2 per uC^2 */
	if ((rc = rat_mul(&f, &f, &k)) != 0)
		return rc;
	if ((rc = dec4_render(f.n, f.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s N", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_electric_field_point(const char *const *args, char *out,
                                      size_t outlen)
{
	rat q, r, k, e;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &q, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &r, NULL)) != 0)
		return rc;
	if (r.n == 0)
		return -4;
	if ((rc = rat_mul(&r, &r, &r)) != 0 ||
	    (rc = rat_div(&e, &q, &r)) != 0)
		return rc;
	k.n = 8990;
	k.d = 1;			/* 8990 N/C per uC at 1 m */
	if ((rc = rat_mul(&e, &e, &k)) != 0)
		return rc;
	if ((rc = dec4_render(e.n, e.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s N/C", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_capacitor_charge(const char *const *args, char *out,
                                  size_t outlen)
{
	rat c, v, q;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &c, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &v, NULL)) != 0)
		return rc;
	if ((rc = rat_mul(&q, &c, &v)) != 0)
		return rc;
	if ((rc = dec4_render(q.n, q.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s uC", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_capacitor_energy(const char *const *args, char *out,
                                  size_t outlen)
{
	rat q, v, e, two;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &q, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &v, NULL)) != 0)
		return rc;
	two.n = 2;
	two.d = 1;
	if ((rc = rat_mul(&e, &q, &v)) != 0 ||
	    (rc = rat_div(&e, &e, &two)) != 0)
		return rc;
	if ((rc = dec4_render(e.n, e.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s uJ", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_parallel_resistance(const char *const *args, char *out,
                                     size_t outlen)
{
	rat r1, r2, num, den, r;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &r1, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &r2, NULL)) != 0)
		return rc;
	if ((rc = rat_mul(&num, &r1, &r2)) != 0 ||
	    (rc = rat_add(&den, &r1, &r2)) != 0)
		return rc;
	if (den.n == 0)
		return -4;
	if ((rc = rat_div(&r, &num, &den)) != 0)
		return rc;
	if ((rc = dec4_render(r.n, r.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s ohm", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_motional_emf(const char *const *args, char *out,
                              size_t outlen)
{
	rat b, l, v, e;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &b, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &l, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &v, NULL)) != 0)
		return rc;
	if ((rc = rat_mul(&e, &b, &l)) != 0 ||
	    (rc = rat_mul(&e, &e, &v)) != 0)
		return rc;
	if ((rc = dec4_render(e.n, e.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s V", f1);
	out_set(out, outlen, buf);
	return 0;
}

int solvers_em_solve(const char *name, const char *const *args, int nargs,
                     char *out, size_t outlen)
{
	int rc;

	if (strcmp(name, "coulomb_force") == 0) {
		if (nargs == 3)
			rc = solve_coulomb_force(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "electric_field_point") == 0) {
		if (nargs == 2)
			rc = solve_electric_field_point(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "capacitor_charge") == 0) {
		if (nargs == 2)
			rc = solve_capacitor_charge(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "capacitor_energy") == 0) {
		if (nargs == 2)
			rc = solve_capacitor_energy(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "parallel_resistance") == 0) {
		if (nargs == 2)
			rc = solve_parallel_resistance(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "motional_emf") == 0) {
		if (nargs == 3)
			rc = solve_motional_emf(args, out, outlen);
		else
			rc = -2;
	} else {
		return 0;
	}
	return rc == 0 ? 1 : rc;
}
