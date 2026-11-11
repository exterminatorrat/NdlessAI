/*
 * Taylor series: the polynomial must approximate its function near the centre,
 * and the printed form must be an expression the parser accepts.
 *
 * Nothing here compares text against a hand-written answer, because the
 * coefficients are fixed point: 1/6 is not 0.166666... and a series the size of
 * a screen cannot be exact. What can be checked honestly is that the series
 * converges to the function on the interval it is meant for, and that printing
 * it does not lose anything.
 */
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "expr.h"

static int failures;

static void fail(const char *what, const char *detail)
{
	printf("FAIL %s: %s\n", what, detail);
	failures++;
}

/* Value of the series at x, and of the function it came from. */
static void compare(const char *text, double a, int terms, double x,
                    double tol)
{
	char err[64];
	char printed[256];
	gexpr_t f, poly, reparsed;
	q16_t xq = (q16_t)(x * Q16_ONE);
	double want, got;

	if (gexpr_parse(text, &f, err, (int)sizeof err) < 0) {
		fail(text, err);
		return;
	}
	if (gexpr_taylor(&f, (q16_t)(a * Q16_ONE), terms, &poly) !=
	    GEXPR_DIFF_OK) {
		char detail[96];

		snprintf(detail, sizeof detail, "no series with %d terms", terms);
		fail(text, detail);
		return;
	}
	got = gexpr_eval(&poly, xq) / (double)Q16_ONE;
	want = gexpr_eval(&f, xq) / (double)Q16_ONE;
	if (fabs(want - got) > tol * (fabs(want) > 1 ? fabs(want) : 1)) {
		char detail[192];

		snprintf(detail, sizeof detail,
		         "%d terms at x=%g gives %g, function is %g",
		         terms, x, got, want);
		fail(text, detail);
		return;
	}

	/* What it prints has to parse back to the same values. */
	if (gexpr_unparse(&poly, printed, (int)sizeof printed) < 0) {
		fail(text, "series does not print");
		return;
	}
	if (gexpr_parse(printed, &reparsed, err, (int)sizeof err) < 0) {
		char detail[320];

		snprintf(detail, sizeof detail, "printed {%s} does not parse: %s",
		         printed, err);
		fail(text, detail);
		return;
	}
	if (labs((long)gexpr_eval(&reparsed, xq) - gexpr_eval(&poly, xq)) > 64) {
		char detail[320];

		snprintf(detail, sizeof detail,
		         "printed {%s} evaluates differently", printed);
		fail(text, detail);
	}
}

static void expect_typed(const char *text, const char *must_contain)
{
	char out[256];

	if (gexpr_series_answer(text, out, (int)sizeof out) < 0) {
		fail(text, "no answer");
		return;
	}
	if (must_contain && !strstr(out, must_contain)) {
		char detail[320];

		snprintf(detail, sizeof detail, "answered {%s}, wanted %s in it",
		         out, must_contain);
		fail(text, detail);
	}
}

static void expect_refused(const char *text)
{
	char out[256];

	if (gexpr_series_answer(text, out, (int)sizeof out) == 0)
		fail(text, "answered a series it cannot build");
}

int main(void)
{
	/* Converges towards the function on the interval the series is for. */
	compare("exp(x)", 0, 6, 0.5, 1e-3);
	compare("exp(x)", 0, 3, 0.5, 0.02);
	compare("sin(x)", 0, 7, 1.0, 2e-3);
	compare("cos(x)", 0, 6, 1.0, 2e-3);
	compare("1/(1-x)", 0, 5, 0.3, 5e-3);
	compare("ln(x+1)", 0, 6, 0.25, 5e-3);

	/* Expanding about a point that is not zero: the series has to carry the
	 * shift, so sin about pi/2 is the cosine series written in (x-pi/2). */
	compare("sin(x)", 1.5707963, 5, 1.4, 5e-3);

	/* More terms is never worse in the interval it is meant for. */
	compare("exp(x)", 0, 8, 0.5, 1e-4);

	/* Through the typed path, as a printed expression. */
	expect_typed("series(exp(x),0,4)", "x");
	expect_typed("taylor(sin(x),0,5)", "x");
	expect_typed("series(1/(1-x),0,4)", NULL);
	/* tan has a derivative rule, so it has a series. */
	expect_typed("series(tan(x),0,4)", "x");

	/*
	 * No derivative rule, so no series. tan and sqrt do have one - they
	 * answer - and a series about a point where the derivative is infinite
	 * is a separate problem, not a missing rule.
	 */
	expect_refused("series(sin(x)^x,0,4)");
	expect_refused("series(x^x,0,4)");
	/* Malformed or out of range. */
	expect_refused("series(exp(x),0)");
	expect_refused("series(exp(x),0,0)");
	expect_refused("series(exp(x),0,99)");
	expect_refused("series(exp(x),0,2.5)");

	if (failures) {
		printf("failures: %d\n", failures);
		return 1;
	}
	printf("all cases passed\n");
	return 0;
}
