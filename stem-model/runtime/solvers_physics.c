#include <stdio.h>
#include <string.h>

#include "solvers.h"
#include "solvers_subject.h"

static int solve_kinematic(const char *const *args, char *out, size_t outlen)
{
	rat v0, a, t, at, r;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &v0, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &a, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &t, NULL)) != 0)
		return rc;
	if ((rc = rat_mul(&at, &a, &t)) != 0 ||
	    (rc = rat_add(&r, &v0, &at)) != 0)
		return rc;
	if ((rc = dec4_render(r.n, r.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s m/s", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_proj(const char *const *args, char *out, size_t outlen)
{
	rat v0, th, g;
	int rc, ev, eg, et;
	long long s9, num, den;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &v0, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &th, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &g, NULL)) != 0)
		return rc;
	ev = pow10_index(v0.d);
	et = pow10_index(th.d);
	eg = pow10_index(g.d);
	if (ev < 0 || et < 0 || eg < 0 || ev > 1 || eg > 2)
		return -4;
	if (v0.n > 3900LL || -v0.n > 3900LL)
		return -4;
	if (g.n > 9200000LL || -g.n > 9200000LL)
		return -4;
	s9 = sin9(2 * th.n, et, &rc);
	if (rc != 0)
		return rc;
	num = (v0.n * v0.n) * s9;
	num *= P10[eg];
	den = g.n * 1000000000LL * P10[2 * ev];
	if (den < 0) {
		den = -den;
		num = -num;
	}
	if ((rc = dec4_render(num, den, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s m", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_ohm(const char *const *args, char *out, size_t outlen)
{
	rat v, r, q;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &v, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &r, NULL)) != 0)
		return rc;
	if ((rc = rat_div(&q, &v, &r)) != 0)
		return rc;
	if ((rc = dec4_render(q.n, q.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s A", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_kinetic_energy(const char *const *args, char *out,
                                size_t outlen)
{
	rat m, v;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &m, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &v, NULL)) != 0)
		return rc;
	if ((rc = dec4_render(m.n * v.n * v.n, 2 * m.d * v.d * v.d,
	                      f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s J", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_potential_energy(const char *const *args, char *out,
                                  size_t outlen)
{
	rat m, h, g;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &m, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &h, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &g, NULL)) != 0)
		return rc;
	if ((rc = dec4_render(m.n * h.n * g.n, m.d * h.d * g.d,
	                      f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s J", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_work(const char *const *args, char *out, size_t outlen)
{
	rat F, d, th;
	int rc, et;
	long long c9, num, den;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &F, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &d, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &th, NULL)) != 0)
		return rc;
	et = pow10_index(th.d);
	if (et < 0 || et > 9)
		return -4;
	c9 = sin9(th.n + 90LL * P10[et], et, &rc);
	if (rc != 0)
		return rc;
	num = F.n * d.n * c9;
	den = F.d * d.d * 1000000000LL;
	if (den < 0) {
		num = -num;
		den = -den;
	}
	if ((rc = dec4_render(num, den, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s J", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_power(const char *const *args, char *out, size_t outlen)
{
	rat W, t;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &W, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &t, NULL)) != 0)
		return rc;
	if (t.n == 0)
		return -4;
	if ((rc = dec4_render(W.n * t.d, W.d * t.n, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s W", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_momentum(const char *const *args, char *out, size_t outlen)
{
	rat m, v;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &m, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &v, NULL)) != 0)
		return rc;
	if ((rc = dec4_render(m.n * v.n, m.d * v.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s kg m/s", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_newton2(const char *const *args, char *out, size_t outlen)
{
	rat m, a;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &m, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &a, NULL)) != 0)
		return rc;
	if ((rc = dec4_render(m.n * a.n, m.d * a.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s N", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_circular_accel(const char *const *args, char *out,
                                size_t outlen)
{
	rat v, r;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &v, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &r, NULL)) != 0)
		return rc;
	if (r.n == 0)
		return -4;
	if ((rc = dec4_render(v.n * v.n * r.d, v.d * v.d * r.n,
	                      f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s m/s^2", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_wave_speed(const char *const *args, char *out, size_t outlen)
{
	rat f, lam;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &f, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &lam, NULL)) != 0)
		return rc;
	if ((rc = dec4_render(f.n * lam.n, f.d * lam.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s m/s", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_specific_heat(const char *const *args, char *out,
                               size_t outlen)
{
	rat m, c, dT;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &m, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &c, NULL)) != 0 ||
	    (rc = parse_rat(args[2], &dT, NULL)) != 0)
		return rc;
	if ((rc = dec4_render(m.n * c.n * dT.n, m.d * c.d * dT.d,
	                      f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s J", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_ohm_v(const char *const *args, char *out, size_t outlen)
{
	rat I, R;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &I, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &R, NULL)) != 0)
		return rc;
	if ((rc = dec4_render(I.n * R.n, I.d * R.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s V", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_ohm_power(const char *const *args, char *out, size_t outlen)
{
	rat I, V;
	int rc;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &I, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &V, NULL)) != 0)
		return rc;
	if ((rc = dec4_render(I.n * V.n, I.d * V.d, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s W", f1);
	out_set(out, outlen, buf);
	return 0;
}

static int solve_free_fall(const char *const *args, char *out, size_t outlen)
{
	rat h, g, s;
	int rc;
	long long s12;
	char f1[48], buf[64];

	if ((rc = parse_rat(args[0], &h, NULL)) != 0 ||
	    (rc = parse_rat(args[1], &g, NULL)) != 0)
		return rc;
	s.n = 2 * h.n * g.n;
	s.d = h.d * g.d;
	if ((rc = rat_norm(&s)) != 0)
		return rc;
	if (s.n < 0)
		return -4;
	s12 = sqrt12(s.n, s.d, &rc);
	if (rc != 0)
		return rc;
	if ((rc = dec4_render(s12, 1000000000000LL, f1, sizeof(f1))) != 0)
		return rc;
	snprintf(buf, sizeof(buf), "%s m/s", f1);
	out_set(out, outlen, buf);
	return 0;
}

int solvers_physics_solve(const char *name, const char *const *args,
                          int nargs, char *out, size_t outlen)
{
	int rc;

	if (strcmp(name, "kinematic_v") == 0) {
		if (nargs == 3)
			rc = solve_kinematic(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "proj_range") == 0) {
		if (nargs == 3)
			rc = solve_proj(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "ohm_i") == 0) {
		if (nargs == 2)
			rc = solve_ohm(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "kinetic_energy") == 0) {
		if (nargs == 2)
			rc = solve_kinetic_energy(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "potential_energy") == 0) {
		if (nargs == 3)
			rc = solve_potential_energy(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "work") == 0) {
		if (nargs == 3)
			rc = solve_work(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "power") == 0) {
		if (nargs == 2)
			rc = solve_power(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "momentum") == 0) {
		if (nargs == 2)
			rc = solve_momentum(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "newton2") == 0) {
		if (nargs == 2)
			rc = solve_newton2(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "circular_accel") == 0) {
		if (nargs == 2)
			rc = solve_circular_accel(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "wave_speed") == 0) {
		if (nargs == 2)
			rc = solve_wave_speed(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "specific_heat") == 0) {
		if (nargs == 3)
			rc = solve_specific_heat(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "ohm_v") == 0) {
		if (nargs == 2)
			rc = solve_ohm_v(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "ohm_power") == 0) {
		if (nargs == 2)
			rc = solve_ohm_power(args, out, outlen);
		else
			rc = -2;
	} else if (strcmp(name, "free_fall") == 0) {
		if (nargs == 2)
			rc = solve_free_fall(args, out, outlen);
		else
			rc = -2;
	} else {
		return 0;
	}
	return rc == 0 ? 1 : rc;
}
