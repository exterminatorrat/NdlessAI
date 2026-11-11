/*
 * Round-trip gate for symbolic integration: differentiate the antiderivative
 * and it has to give the integrand back at every sample point.
 *
 * This is the check that matters for the rules, because it needs no table of
 * expected antiderivatives: a rule that is off by a factor differentiates into
 * something close to the integrand, which is exactly the failure a
 * hand-written expected answer would also have copied down.
 *
 * Not every expression integrates, and a refusal is a pass - but an expression
 * that claims to integrate must survive the round trip.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "expr.h"

static int failures;

static void fail(const char *what, const char *detail)
{
	printf("FAIL %s: %s\n", what, detail);
	failures++;
}

static int close_enough(double got, double want)
{
	double diff = fabs(got - want);

	if (diff <= 1e-6)
		return 1;
	return diff <= 3e-3 * fabs(want);
}

static const q16_t samples[] = { 1, 2, 3, 4, 6, 8 };

static void round_trip(const char *text, int must_integrate)
{
	char err[64];
	char sym[128];
	gexpr_t f, anti, back;
	int i;

	if (gexpr_parse(text, &f, err, (int)sizeof err) < 0) {
		fail(text, err);
		return;
	}
	if (gexpr_integrate_symbolic(&f, &anti) != GEXPR_DIFF_OK) {
		if (must_integrate)
			fail(text, "refused an expression it should integrate");
		return;
	}
	if (gexpr_unparse(&anti, sym, (int)sizeof sym) < 0) {
		fail(text, "antiderivative does not print");
		return;
	}
	if (gexpr_derive(&anti, &back) != GEXPR_DIFF_OK) {
		fail(text, "antiderivative does not differentiate");
		return;
	}

	for (i = 0; i < (int)(sizeof samples / sizeof samples[0]); i++) {
		double x = samples[i] / 2.0; /* 0.5, 1, 1.5, 2, 3, 4 */
		q16_t xa = (q16_t)(x * Q16_ONE);
		double want = gexpr_eval(&f, xa) / (double)Q16_ONE;
		double got = gexpr_eval(&back, xa) / (double)Q16_ONE;

		if (!close_enough(got, want)) {
			char detail[256];

			snprintf(detail, sizeof detail,
			         "{%s} differentiates to %g at x=%g, want %g",
			         sym, got, x, want);
			fail(text, detail);
			return;
		}
	}
}

static void expect_answer(const char *text, const char *want)
{
	char err[64];
	char sym[128];
	gexpr_t f, anti;

	if (gexpr_parse(text, &f, err, (int)sizeof err) < 0) {
		fail(text, err);
		return;
	}
	if (gexpr_integrate_symbolic(&f, &anti) != GEXPR_DIFF_OK) {
		fail(text, "refused");
		return;
	}
	if (gexpr_unparse(&anti, sym, (int)sizeof sym) < 0) {
		fail(text, "does not print");
		return;
	}
	if (strcmp(sym, want) != 0) {
		char detail[256];

		snprintf(detail, sizeof detail, "printed {%s}, want {%s}",
		         sym, want);
		fail(text, detail);
	}
}

static void expect_refused(const char *text)
{
	char err[64];
	gexpr_t f, anti;

	if (gexpr_parse(text, &f, err, (int)sizeof err) < 0) {
		fail(text, err);
		return;
	}
	if (gexpr_integrate_symbolic(&f, &anti) == GEXPR_DIFF_OK)
		fail(text, "integrated something it has no rule for");
}

/* The indefinite typed form, end to end through the answer path. */
static void expect_typed(const char *text, const char *want)
{
	char out[256];

	if (gexpr_integral_answer(text, out, (int)sizeof out) < 0) {
		fail(text, "no answer");
		return;
	}
	if (strcmp(out, want) != 0) {
		char detail[256];

		snprintf(detail, sizeof detail, "answered {%s}, want {%s}",
		         out, want);
		fail(text, detail);
	}
}

int main(void)
{
	/* Every rule that exists, and the shapes they compose into. */
	round_trip("x^2", 1);
	round_trip("x", 1);
	round_trip("5", 1);
	round_trip("2x", 1);
	round_trip("3x^2+2x-7", 1);
	round_trip("4x^2-3x+1", 1);
	round_trip("x^3/3", 1);
	round_trip("x^5", 1);
	round_trip("2x^3-4x", 1);
	round_trip("sin(x)", 1);
	round_trip("cos(x)", 1);
	round_trip("exp(x)", 1);
	round_trip("sin(3x)", 1);
	round_trip("cos(2x)", 1);
	round_trip("-x^2", 1);
	round_trip("x^2+x", 1);

	/* The shapes it must print, because that is what a student reads. */
	expect_answer("x^2", "x^3/3");
	expect_answer("x", "x^2/2");
	expect_answer("3x^2+2x-7", "x^3+x^2-7x");
	expect_answer("1/x", "ln(x)");
	expect_answer("cos(x)", "sin(x)");
	expect_answer("sin(x)", "-cos(x)");
	expect_answer("sin(4x)", "-cos(4x)/4");

	/* No rule: refusing beats a plausible wrong answer. */
	expect_refused("tan(x)");
	expect_refused("sqrt(x)");
	expect_refused("ln(x)");
	expect_refused("sin(x^2)");
	expect_refused("x*x");

	/* Through the typed answer path, with the constant of integration. */
	expect_typed("integrate(x^2)", "x^3/3 + C");
	expect_typed("integrate(3x^2+2x-7)", "x^3+x^2-7x + C");
	expect_typed("int(2x)", "x^2 + C");
	/* The bounded form still answers a number rather than an expression. */
	expect_typed("integrate(x^2,0,3)", "9");

	if (failures) {
		printf("failures: %d\n", failures);
		return 1;
	}
	printf("all cases passed\n");
	return 0;
}
