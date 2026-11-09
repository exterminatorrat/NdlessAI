/*
 * Host tests for the equation engine.
 *
 * The calculator cannot be debugged interactively, so the reduction, the
 * degree-1/degree-2 solution and the two transform families are pinned down
 * here. Every solved case also substitutes its roots back through gexpr_eval,
 * which is the check that catches a solver that is self-consistent but wrong.
 *
 *   ./test_equation     run the checks
 */
#include <stdio.h>
#include <string.h>

#include "equation.h"

/* Q16 units a substitution may be off; both sides are evaluated in Q16. */
#define TOL 64

static int failures;

static void check(int cond, const char *what)
{
	if (!cond) {
		printf("FAIL %s\n", what);
		failures++;
	}
}

static const char *status_name(eq_status_t st)
{
	switch (st) {
	case EQ_SOLVED:
		return "solved";
	case EQ_NONE:
		return "none";
	case EQ_ALL:
		return "all";
	case EQ_UNSUPPORTED:
		return "unsupported";
	case EQ_OVERFLOW:
		return "overflow";
	}
	return "?";
}

static int load(const char *text, gexpr_t *e)
{
	char err[64];

	if (gexpr_parse(text, e, err, sizeof(err)) != 0) {
		printf("FAIL parse \"%s\": %s\n", text, err);
		failures++;
		return -1;
	}
	return 0;
}

/*
 * Root as Q16. The whole part is bounded first, then the fraction is scaled
 * with a shrinking denominator so a wide rational cannot overflow int64.
 */
static q16_t to_q16(const rat *r)
{
	long long whole = r->n / r->d;
	long long rem = r->n % r->d;
	long long d = r->d;

	if (whole > 32768 || whole < -32768)
		return whole > 0 ? Q16_MAX : Q16_MIN;
	while (d > 140000000000000LL) {
		rem /= 2;
		d /= 2;
	}
	return gq_sat(whole * 65536 + (rem * 65536) / d);
}

static void expect_root(const eq_result_t *res, int i, long long n, long long d,
                        const char *what)
{
	rat want;

	want.n = n;
	want.d = d;
	check(res->n > i && eq_rat_cmp(&res->root[i], &want) == 0, what);
}

static void expect_near(const eq_result_t *res, int i, long long num,
                        long long den, const char *what)
{
	rat want;
	long long got, diff;

	want.n = num;
	want.d = den;
	got = to_q16(&res->root[i]);
	diff = got - to_q16(&want);
	if (diff < 0)
		diff = -diff;
	check(res->n > i && diff <= 2, what);
}

static void check_substitutes(const gexpr_t *lhs, const gexpr_t *rhs,
                              const eq_result_t *res, const char *label)
{
	int i;

	for (i = 0; i < res->n; i++) {
		q16_t x = to_q16(&res->root[i]);
		long long d = (long long)gexpr_eval(lhs, x) - gexpr_eval(rhs, x);
		char what[192];

		if (d < 0)
			d = -d;
		printf("  %s root %d x=%9lld/%-9lld q16=%6lld residual %lld\n",
		       label, i, res->root[i].n, res->root[i].d, (long long)x, d);
		snprintf(what, sizeof(what), "%s: root %d substitutes to zero",
		         label, i);
		check(d <= TOL, what);
	}
}

/* Run one solved case and report what it produced. */
static int solved(const char *lhs_text, const char *rhs_text, eq_result_t *res,
                  gexpr_t *lhs, gexpr_t *rhs, const char *label)
{
	memset(res, 0, sizeof(*res));
	if (load(lhs_text, lhs) || load(rhs_text, rhs))
		return -1;
	equation_solve(lhs, rhs, res);
	printf("%-16s = %-12s %-11s n=%d approx=%d\n", lhs_text, rhs_text,
	       status_name(res->status), res->n, res->approx);
	check(res->status == EQ_SOLVED, label);
	return res->status == EQ_SOLVED ? 0 : -1;
}

static void case_linear(void)
{
	gexpr_t lhs, rhs;
	eq_result_t res;

	if (solved("3x + 6", "0", &res, &lhs, &rhs, "3x+6=0 has a solution") == 0) {
		check(res.n == 1, "3x+6=0 has one root");
		check(!res.approx, "3x+6=0 root is exact");
		expect_root(&res, 0, -2, 1, "3x+6=0 root is -2");
		check_substitutes(&lhs, &rhs, &res, "3x+6=0");
	}
	if (solved("x/2 - 3", "0", &res, &lhs, &rhs, "x/2-3=0 has a solution") == 0) {
		expect_root(&res, 0, 6, 1, "x/2-3=0 root is 6");
		check_substitutes(&lhs, &rhs, &res, "x/2-3=0");
	}
}

static void case_degenerate(void)
{
	gexpr_t lhs, rhs;
	eq_result_t res;

	/* 0x = 5: no value works, and that is proved, not guessed. */
	if (load("0x", &lhs) || load("5", &rhs))
		return;
	equation_solve(&lhs, &rhs, &res);
	printf("%-16s = %-12s %-11s n=%d\n", "0x", "5", status_name(res.status),
	       res.n);
	check(res.status == EQ_NONE, "0x=5 has no solution");
	check(res.n == 0, "0x=5 reports no roots");

	/* 0x = 0: every value works. */
	if (load("0x", &lhs) || load("0", &rhs))
		return;
	equation_solve(&lhs, &rhs, &res);
	printf("%-16s = %-12s %-11s n=%d\n", "0x", "0", status_name(res.status),
	       res.n);
	check(res.status == EQ_ALL, "0x=0 holds for every x");

	/* 3(x+2) = 3x+6: product expansion makes the two sides identical. */
	if (load("3(x + 2)", &lhs) || load("3x + 6", &rhs))
		return;
	equation_solve(&lhs, &rhs, &res);
	printf("%-16s = %-12s %-11s n=%d\n", "3(x+2)", "3x+6",
	       status_name(res.status), res.n);
	check(res.status == EQ_ALL, "3(x+2)=3x+6 holds for every x");
}

static void case_quadratic(void)
{
	gexpr_t lhs, rhs;
	eq_result_t res;

	if (solved("x^2 - 5x + 6", "0", &res, &lhs, &rhs, "x^2-5x+6=0") == 0) {
		check(res.n == 2, "x^2-5x+6=0 has two roots");
		check(!res.approx, "x^2-5x+6=0 roots are exact");
		expect_root(&res, 0, 2, 1, "x^2-5x+6=0 first root is 2");
		expect_root(&res, 1, 3, 1, "x^2-5x+6=0 second root is 3");
		check_substitutes(&lhs, &rhs, &res, "x^2-5x+6=0");
	}
	if (solved("2x^2 + 10x + 12", "0", &res, &lhs, &rhs, "2x^2+10x+12=0") == 0) {
		expect_root(&res, 0, -3, 1, "2x^2+10x+12=0 first root is -3");
		expect_root(&res, 1, -2, 1, "2x^2+10x+12=0 second root is -2");
		check_substitutes(&lhs, &rhs, &res, "2x^2+10x+12=0");
	}
	/* A double root is one root, not two copies of it. */
	if (solved("x^2 - 2x + 1", "0", &res, &lhs, &rhs, "x^2-2x+1=0") == 0) {
		check(res.n == 1, "x^2-2x+1=0 has one root");
		expect_root(&res, 0, 1, 1, "x^2-2x+1=0 root is 1");
		check_substitutes(&lhs, &rhs, &res, "x^2-2x+1=0");
	}
	/* Negative discriminant: none, and not "unsupported". */
	if (load("x^2 + 1", &lhs) || load("0", &rhs))
		return;
	equation_solve(&lhs, &rhs, &res);
	printf("%-16s = %-12s %-11s n=%d\n", "x^2+1", "0",
	       status_name(res.status), res.n);
	check(res.status == EQ_NONE, "x^2+1=0 has no real solution");
	/* Irrational roots come back as decimals, not as a failure. */
	if (solved("x^2 - 2", "0", &res, &lhs, &rhs, "x^2-2=0") == 0) {
		expect_near(&res, 0, -1414213562LL, 1000000000LL,
		            "x^2-2=0 first root is -sqrt 2");
		expect_near(&res, 1, 1414213562LL, 1000000000LL,
		            "x^2-2=0 second root is sqrt 2");
		check(res.approx, "x^2-2=0 roots are marked approximate");
		check_substitutes(&lhs, &rhs, &res, "x^2-2=0");
	}
}

static void case_exponential(void)
{
	gexpr_t lhs, rhs;
	eq_result_t res;

	if (solved("3*2^(x + 1)", "24", &res, &lhs, &rhs, "3*2^(x+1)=24") == 0) {
		check(res.n == 1, "3*2^(x+1)=24 has one root");
		check(res.approx, "3*2^(x+1)=24 root is approximate");
		expect_near(&res, 0, 2, 1, "3*2^(x+1)=24 root is 2");
		check_substitutes(&lhs, &rhs, &res, "3*2^(x+1)=24");
	}
	/* A base below 1 gives the same rule with a negative quotient of logs. */
	if (solved("2*0.5^x", "8", &res, &lhs, &rhs, "2*0.5^x=8") == 0) {
		expect_near(&res, 0, -2, 1, "2*0.5^x=8 root is -2");
		check_substitutes(&lhs, &rhs, &res, "2*0.5^x=8");
	}
	/* A coefficient on x in the exponent stays inside the same transform. */
	if (solved("5*3^(2x)", "45", &res, &lhs, &rhs, "5*3^(2x)=45") == 0) {
		expect_near(&res, 0, 1, 1, "5*3^(2x)=45 root is 1");
		check_substitutes(&lhs, &rhs, &res, "5*3^(2x)=45");
	}
	/* The sides are matched either way round, so 8 = 2^x answers too. */
	if (solved("8", "2^x", &res, &lhs, &rhs, "8=2^x") == 0) {
		expect_near(&res, 0, 3, 1, "8=2^x root is 3");
		check_substitutes(&lhs, &rhs, &res, "8=2^x");
	}
	/* A positive base never reaches a negative value: proved, not unknown. */
	if (load("2^x", &lhs) || load("-8", &rhs))
		return;
	equation_solve(&lhs, &rhs, &res);
	printf("%-16s = %-12s %-11s n=%d\n", "2^x", "-8",
	       status_name(res.status), res.n);
	check(res.status == EQ_NONE, "2^x=-8 has no solution");
}

static void case_power(void)
{
	gexpr_t lhs, rhs;
	eq_result_t res;

	if (solved("2*x^3", "16", &res, &lhs, &rhs, "2x^3=16") == 0) {
		check(res.approx, "2x^3=16 root is approximate");
		expect_near(&res, 0, 2, 1, "2x^3=16 root is 2");
		check_substitutes(&lhs, &rhs, &res, "2x^3=16");
	}
	if (solved("x^3", "-27", &res, &lhs, &rhs, "x^3=-27") == 0) {
		expect_near(&res, 0, -3, 1, "x^3=-27 root is -3");
		check_substitutes(&lhs, &rhs, &res, "x^3=-27");
	}
	/* An even power has both signs, and both are reported. */
	if (solved("x^4", "16", &res, &lhs, &rhs, "x^4=16") == 0) {
		check(res.n == 2, "x^4=16 has two roots");
		expect_near(&res, 0, -2, 1, "x^4=16 first root is -2");
		expect_near(&res, 1, 2, 1, "x^4=16 second root is 2");
		check_substitutes(&lhs, &rhs, &res, "x^4=16");
	}
}

static void case_unsupported(void)
{
	gexpr_t lhs, rhs;
	eq_result_t res;
	static const char *const left[] = { "sin(x)", "x^3 + x", "ln(x)",
	                                    "x^3*x^7" };
	static const char *const right[] = { "0", "1", "1", "1" };
	size_t i;

	for (i = 0; i < sizeof(left) / sizeof(left[0]); i++) {
		char what[128];

		if (load(left[i], &lhs) || load(right[i], &rhs))
			return;
		equation_solve(&lhs, &rhs, &res);
		printf("%-16s = %-12s %-11s n=%d\n", left[i], right[i],
		       status_name(res.status), res.n);
		snprintf(what, sizeof(what), "%s = %s is unsupported, not unsolvable",
		         left[i], right[i]);
		check(res.status == EQ_UNSUPPORTED, what);
		check(res.status != EQ_NONE, what);
	}
}

static void case_reduction(void)
{
	gexpr_t lhs, rhs;
	eq_poly_t p;
	int rc;

	if (load("3x^2 - 2x + 1", &lhs) || load("0", &rhs))
		return;
	rc = equation_poly(&lhs, &rhs, &p);
	check(rc == 0 && p.degree == 2, "3x^2-2x+1 reduces to degree 2");
	check(p.degree == 2 && p.c[0].n == 1 && p.c[0].d == 1,
	      "constant coefficient is 1");
	check(p.degree == 2 && p.c[1].n == -2 && p.c[1].d == 1,
	      "linear coefficient is -2");
	check(p.degree == 2 && p.c[2].n == 3 && p.c[2].d == 1,
	      "quadratic coefficient is 3");
	/* Moving the right side across is part of the reduction. */
	if (load("x^2", &lhs) || load("2x - 1", &rhs))
		return;
	rc = equation_poly(&lhs, &rhs, &p);
	check(rc == 0 && p.degree == 2, "x^2=2x-1 reduces to degree 2");
	check(p.degree == 2 && p.c[0].n == 1 && p.c[1].n == -2 &&
	      p.c[2].n == 1, "x^2-2x+1 has coefficients 1, -2, 1");
}

int main(void)
{
	case_reduction();
	case_linear();
	case_degenerate();
	case_quadratic();
	case_exponential();
	case_power();
	case_unsupported();
	printf("%s: %d failure%s\n", failures == 0 ? "PASS" : "FAIL", failures,
	       failures == 1 ? "" : "s");
	return failures == 0 ? 0 : 1;
}
