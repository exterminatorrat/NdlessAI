#include <stdio.h>
#include <string.h>

#include "solvers.h"
#include "solvers_subject.h"

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

static int solve_dilution_v1(const char *const *args, char *out,
                             size_t outlen)
{
	rat m1, m2, v2, t, q;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &m1, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &m2, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &v2, NULL)) != 0)
		return rc;
	if (m1.n == 0)
		return -4;
	if ((rc = rat_mul(&t, &m2, &v2)) != 0 ||
	    (rc = rat_div(&q, &t, &m1)) != 0)
		return rc;
	if ((rc = dec4_render(q.n, q.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s mL", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_molality(const char *const *args, char *out, size_t outlen)
{
	rat n, kg, q;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &n, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &kg, NULL)) != 0)
		return rc;
	if (kg.n == 0)
		return -4;
	if ((rc = rat_div(&q, &n, &kg)) != 0)
		return rc;
	if ((rc = dec4_render(q.n, q.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s mol/kg", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_h_from_ph(const char *const *args, char *out, size_t outlen)
{
	rat ph;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &ph, NULL)) != 0)
		return rc;
	if (ph.d != 1 || ph.n < 0 || ph.n > 17)
		return -4;
	if ((rc = dec4_render(1, P10[ph.n], f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s mol/L", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_buffer_ph(const char *const *args, char *out, size_t outlen)
{
	rat pka, base, acid, ratio;
	long long l9, num, den;
	int rc;
	char f1[48];

	if ((rc = parse_rat(args[0], &pka, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &base, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &acid, NULL)) != 0)
		return rc;
	if (base.n <= 0 || acid.n <= 0)
		return -4;
	if ((rc = rat_div(&ratio, &base, &acid)) != 0)
		return rc;
	l9 = log10_9(ratio.n, ratio.d, &rc);
	if (rc != 0)
		return rc;
	num = pka.n * 1000000000LL + pka.d * l9;
	den = pka.d * 1000000000LL;
	if ((rc = dec4_render(num, den, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

static int solve_calorimetry_mass(const char *const *args, char *out,
                                  size_t outlen)
{
	rat q, c, dT, t, m;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &q, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &c, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &dT, NULL)) != 0)
		return rc;
	if ((rc = rat_mul(&t, &c, &dT)) != 0)
		return rc;
	if (t.n == 0)
		return -4;
	if ((rc = rat_div(&m, &q, &t)) != 0)
		return rc;
	if ((rc = dec4_render(m.n, m.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s g", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_calorimetry_c(const char *const *args, char *out,
                               size_t outlen)
{
	rat q, m, dT, t, c;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &q, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &m, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &dT, NULL)) != 0)
		return rc;
	if ((rc = rat_mul(&t, &m, &dT)) != 0)
		return rc;
	if (t.n == 0)
		return -4;
	if ((rc = rat_div(&c, &q, &t)) != 0)
		return rc;
	if ((rc = dec4_render(c.n, c.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s J/(g C)", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_latent_heat(const char *const *args, char *out, size_t outlen)
{
	rat m, L, q;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &m, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &L, NULL)) != 0)
		return rc;
	if ((rc = rat_mul(&q, &m, &L)) != 0)
		return rc;
	if ((rc = dec4_render(q.n, q.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s J", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_first_law(const char *const *args, char *out, size_t outlen)
{
	rat q, w, du;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &q, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &w, NULL)) != 0)
		return rc;
	if ((rc = rat_add(&du, &q, &w)) != 0)
		return rc;
	if ((rc = dec4_render(du.n, du.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s J", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_gibbs(const char *const *args, char *out, size_t outlen)
{
	rat dh, t, ds, thousand, p, g;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &dh, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &t, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &ds, NULL)) != 0)
		return rc;
	thousand.n = 1000;
	thousand.d = 1;
	if ((rc = rat_mul(&p, &t, &ds)) != 0 ||
	    (rc = rat_div(&p, &p, &thousand)) != 0 ||
	    (rc = rat_sub(&g, &dh, &p)) != 0)
		return rc;
	if ((rc = dec4_render(g.n, g.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s kJ", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_carnot(const char *const *args, char *out, size_t outlen)
{
	rat th, tc, d, hundred, p;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &th, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &tc, NULL)) != 0)
		return rc;
	if (th.n == 0)
		return -4;
	hundred.n = 100;
	hundred.d = 1;
	if ((rc = rat_sub(&d, &th, &tc)) != 0 ||
	    (rc = rat_mul(&p, &d, &hundred)) != 0 ||
	    (rc = rat_div(&p, &p, &th)) != 0)
		return rc;
	if ((rc = dec4_render(p.n, p.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s%%", f1);
	out_set(out, outlen, buf);
	return 0;
}

int solvers_chem_solve(const char *name, const char *const *args,
                       int nargs, char *out, size_t outlen)
{
	int rc;

	if (strcmp(name, "moles") == 0) {
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
	} else if (strcmp(name, "dilution_v1") == 0) {
		if (nargs == 3)
			rc = solve_dilution_v1(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "molality") == 0) {
		if (nargs == 2)
			rc = solve_molality(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "h_from_ph") == 0) {
		if (nargs == 1)
			rc = solve_h_from_ph(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "buffer_ph") == 0) {
		if (nargs == 3)
			rc = solve_buffer_ph(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "calorimetry_mass") == 0) {
		if (nargs == 3)
			rc = solve_calorimetry_mass(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "calorimetry_c") == 0) {
		if (nargs == 3)
			rc = solve_calorimetry_c(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "latent_heat") == 0) {
		if (nargs == 2)
			rc = solve_latent_heat(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "first_law") == 0) {
		if (nargs == 2)
			rc = solve_first_law(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "gibbs") == 0) {
		if (nargs == 3)
			rc = solve_gibbs(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "carnot") == 0) {
		if (nargs == 2)
			rc = solve_carnot(args, out, outlen);
		else
			rc = -2;
	} else {
		return 0;
	}
	return rc == 0 ? 1 : rc;
}
