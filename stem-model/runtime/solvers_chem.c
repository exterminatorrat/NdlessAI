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

static int solve_mass_percent(const char *const *args, char *out,
                              size_t outlen)
{
	rat solute, solution, hundred, p;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &solute, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &solution, NULL)) != 0)
		return rc;
	if (solution.n == 0)
		return -4;
	hundred.n = 100;
	hundred.d = 1;
	if ((rc = rat_mul(&p, &solute, &hundred)) != 0 ||
	    (rc = rat_div(&p, &p, &solution)) != 0)
		return rc;
	if ((rc = dec4_render(p.n, p.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s%%", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_stoich_mass(const char *const *args, char *out, size_t outlen)
{
	rat mass, mm_given, mm_product, ratio, moles, p;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &mass, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &mm_given, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &mm_product, NULL)) != 0 ||
	    (rc = parse_rat(args[3], &ratio, NULL)) != 0)
		return rc;
	if (mm_given.n == 0)
		return -4;
	if ((rc = rat_div(&moles, &mass, &mm_given)) != 0 ||
	    (rc = rat_mul(&p, &moles, &ratio)) != 0 ||
	    (rc = rat_mul(&p, &p, &mm_product)) != 0)
		return rc;
	if ((rc = dec4_render(p.n, p.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s g", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_mole_fraction(const char *const *args, char *out,
                               size_t outlen)
{
	rat na, nb, total, p;
	int rc;
	char f1[48];

	if ((rc = parse_rat(args[0], &na, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &nb, NULL)) != 0)
		return rc;
	if ((rc = rat_add(&total, &na, &nb)) != 0)
		return rc;
	if (total.n == 0)
		return -4;
	if ((rc = rat_div(&p, &na, &total)) != 0)
		return rc;
	if ((rc = dec4_render(p.n, p.d, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

static int solve_kc_from_conc(const char *const *args, char *out,
                              size_t outlen)
{
	rat product, reactant, p;
	int rc, coef;
	char f1[48];

	if ((rc = parse_rat(args[0], &product, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &reactant, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &p, NULL)) != 0)
		return rc;
	if (reactant.n == 0)
		return -4;
	coef = (int)(p.n / p.d);
	if (coef < 1 || coef > 4)
		return -4;
	p = product;
	for (int i = 1; i < coef; i++)
		if ((rc = rat_mul(&p, &p, &product)) != 0)
			return rc;
	if ((rc = rat_div(&p, &p, &reactant)) != 0)
		return rc;
	if ((rc = dec4_render(p.n, p.d, f1, sizeof(f1))) != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

static int solve_avg_rate(const char *const *args, char *out, size_t outlen)
{
	rat change, time, p;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &change, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &time, NULL)) != 0)
		return rc;
	if (time.n == 0)
		return -4;
	if ((rc = rat_div(&p, &change, &time)) != 0)
		return rc;
	if ((rc = dec4_render(p.n, p.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s M/s", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_heat_reaction(const char *const *args, char *out,
                               size_t outlen)
{
	rat moles, enthalpy, p;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &moles, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &enthalpy, NULL)) != 0)
		return rc;
	if ((rc = rat_mul(&p, &moles, &enthalpy)) != 0)
		return rc;
	if ((rc = dec4_render(p.n, p.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s kJ", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_limiting_moles(const char *const *args, char *out,
                                size_t outlen)
{
	rat ma, mma, mb, mmb;
	int rc;
	char f1[48];

	if ((rc = parse_rat(args[0], &ma, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &mma, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &mb, NULL)) != 0 ||
	    (rc = parse_rat(args[3], &mmb, NULL)) != 0)
		return rc;
	if (mma.n == 0 || mmb.n == 0)
		return -4;
	if (ma.n * mma.d * mmb.n * mb.d <= mb.n * mmb.d * mma.n * ma.d)
		rc = dec4_render(ma.n * mma.d, ma.d * mma.n, f1, sizeof(f1));
	else
		rc = dec4_render(mb.n * mmb.d, mb.d * mmb.n, f1, sizeof(f1));
	if (rc != 0)
		return rc;
	out_set(out, outlen, f1);
	return 0;
}

static int solve_partial_pressure(const char *const *args, char *out,
                                  size_t outlen)
{
	rat n_i, n_total, p_total, t, p;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &n_i, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &n_total, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &p_total, NULL)) != 0)
		return rc;
	if (n_total.n == 0)
		return -4;
	if ((rc = rat_mul(&t, &n_i, &p_total)) != 0 ||
	    (rc = rat_div(&p, &t, &n_total)) != 0)
		return rc;
	if ((rc = dec4_render(p.n, p.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s atm", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_cell_potential(const char *const *args, char *out,
                                size_t outlen)
{
	rat e_cat, e_an, e_cell;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &e_cat, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &e_an, NULL)) != 0)
		return rc;
	if ((rc = rat_sub(&e_cell, &e_cat, &e_an)) != 0)
		return rc;
	if ((rc = dec4_render(e_cell.n, e_cell.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s V", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_ksp_from_solubility(const char *const *args, char *out,
                                     size_t outlen)
{
	rat s, n, m;
	long long num = 1, den = 1, g;
	int rc, i, in, im;

	if ((rc = parse_rat(args[0], &s, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &n, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &m, NULL)) != 0)
		return rc;
	if ((rc = rat_norm(&s)) != 0)
		return rc;
	if (s.n <= 0 || s.d <= 0 || n.d != 1 || m.d != 1 ||
	    n.n < 1 || n.n > 8 || m.n < 1 || m.n > 8)
		return -4;
	in = (int)n.n;
	im = (int)m.n;

	/*
	 * Ksp = (in*s)^in * (im*s)^im with s = sn/sd, so the exact answer is
	 * in^in * im^im * sn^(in+im) / sd^(in+im). Built as integers rather than
	 * through rat_mul: rat_norm refuses a numerator or denominator above
	 * 2e9, and a Ksp of 1e-14 is a denominator of 1e14. The three factors
	 * are multiplied with an overflow guard, reduced once, and only then
	 * measured against what the renderer accepts, which is the same test the
	 * catalogue applies to the reduced rational.
	 */
	for (i = 0; i < in; i++) {
		if (num > P10[17] / in)
			return -4;
		num *= in;
	}
	for (i = 0; i < im; i++) {
		if (num > P10[17] / im)
			return -4;
		num *= im;
	}
	for (i = 0; i < in + im; i++) {
		if (num > P10[18] / s.n || den > P10[18] / s.d)
			return -4;
		num *= s.n;
		den *= s.d;
	}
	g = gcd_ll(num, den);
	num /= g;
	den /= g;
	if (num > WIDE_LIMIT || den > P10[17])
		return -4;
	if ((rc = dec4_render(num, den, out, outlen)) != 0)
		return rc;
	return 0;
}

static int solve_beer_lambert(const char *const *args, char *out,
                              size_t outlen)
{
	rat a, eps, l, el, c;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &a, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &eps, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &l, NULL)) != 0)
		return rc;
	if ((rc = rat_mul(&el, &eps, &l)) != 0)
		return rc;
	if (el.n == 0)
		return -4;
	if ((rc = rat_div(&c, &a, &el)) != 0)
		return rc;
	if ((rc = dec4_render(c.n, c.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s mol/L", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_atoms_from_mass(const char *const *args, char *out,
                                 size_t outlen)
{
	rat mass, mm, r, avo = { 6022, 1000 };
	int rc, e = 0;
	char f1[48], buf[72];

	if ((rc = parse_rat(args[0], &mass, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &mm, NULL)) != 0)
		return rc;
	if (mass.n <= 0 || mm.n <= 0)
		return -4;
	if ((rc = rat_mul(&r, &mass, &avo)) != 0 ||
	    (rc = rat_div(&r, &r, &mm)) != 0)
		return rc;

	/*
	 * N = (m/M) * 6.022e23, which is around 1e22 and cannot be one rational
	 * for dec4_render: its numerator stops at 9e15. Shifting the exact
	 * rational into [1, 10) before rounding keeps the four printed figures
	 * identical to the catalogue's dec_exact, and the exponent is printed
	 * rather than computed.
	 */
	while (r.n >= r.d * 10) {
		if (r.d > P10[17] / 10)
			return -4;
		r.d *= 10;
		e++;
	}
	while (r.n < r.d) {
		if (r.n > WIDE_LIMIT / 10)
			return -4;
		r.n *= 10;
		e--;
	}
	if ((rc = dec4_render(r.n, r.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%se+%02d atoms", f1, 23 + e);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_wavelength_from_frequency(const char *const *args, char *out,
                                           size_t outlen)
{
	rat f;
	long long den;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &f, NULL)) != 0)
		return rc;
	/* The argument is in THz, so lambda = 2.998e8 / (f * 1e12) m, which is
	 * 2998 / (1e7 * f). Built as integers for the same reason as Ksp: the
	 * answer is a few hundred nanometres, far below rat_norm's 2e9 floor. */
	if (f.n <= 0 || f.d != 1 || f.n > P10[17] / 10000000LL)
		return -4;
	den = f.n * 10000000LL;
	if ((rc = dec4_render(2998, den, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s m", f1);
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
	} else if (strcmp(name, "mass_percent") == 0) {
		if (nargs == 2)
			rc = solve_mass_percent(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "stoich_mass") == 0) {
		if (nargs == 4)
			rc = solve_stoich_mass(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "mole_fraction") == 0) {
		if (nargs == 2)
			rc = solve_mole_fraction(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "kc_from_conc") == 0) {
		if (nargs == 3)
			rc = solve_kc_from_conc(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "avg_rate") == 0) {
		if (nargs == 2)
			rc = solve_avg_rate(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "limiting_moles") == 0) {
		if (nargs == 4)
			rc = solve_limiting_moles(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "heat_reaction") == 0) {
		if (nargs == 2)
			rc = solve_heat_reaction(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "partial_pressure") == 0) {
		if (nargs == 3)
			rc = solve_partial_pressure(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "cell_potential") == 0) {
		if (nargs == 2)
			rc = solve_cell_potential(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "ksp_from_solubility") == 0) {
		if (nargs == 3)
			rc = solve_ksp_from_solubility(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "beer_lambert") == 0) {
		if (nargs == 3)
			rc = solve_beer_lambert(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "atoms_from_mass") == 0) {
		if (nargs == 2)
			rc = solve_atoms_from_mass(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "wavelength_from_frequency") == 0) {
		if (nargs == 1)
			rc = solve_wavelength_from_frequency(args, out, outlen);
		else
			rc = -2;
	} else {
		return 0;
	}
	return rc == 0 ? 1 : rc;
}
