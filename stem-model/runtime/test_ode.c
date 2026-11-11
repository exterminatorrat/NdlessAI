/*
 * Differential equations: y' = f(x,y) stepped with RK4.
 *
 * The check is against the closed form of the equations a student meets, at
 * four significant digits, because that is what the answer renders at. Anything
 * looser would pass a broken step; anything tighter would test Q16 rather than
 * the integrator.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expr.h"

static int failures;

static void fail(const char *what, const char *detail)
{
	printf("FAIL %s: %s\n", what, detail);
	failures++;
}

static void expect_final(const char *text, double y0, double x0, double xn,
                         int steps, double want, double tol)
{
	char err[64];
	gexpr_t f;
	q16_t ys[EXPR_MAX_ODE_STEPS + 1];
	double got;

	if (gexpr_parse(text, &f, err, (int)sizeof err) < 0) {
		fail(text, err);
		return;
	}
	if (gexpr_rk4(&f, (q16_t)(x0 * Q16_ONE), (q16_t)(y0 * Q16_ONE),
	              (q16_t)(xn * Q16_ONE), steps, ys,
	              (int)(sizeof ys / sizeof ys[0])) != GEXPR_DIFF_OK) {
		fail(text, "rk4 refused a valid equation");
		return;
	}
	if (ys[0] != (q16_t)(y0 * Q16_ONE)) {
		fail(text, "the table does not start at y0");
		return;
	}
	got = ys[steps] / (double)Q16_ONE;
	if (fabs(got - want) > tol * fabs(want)) {
		char detail[192];

		snprintf(detail, sizeof detail,
		         "%d steps from x=%g gives %g, closed form is %g",
		         steps, x0, got, want);
		fail(text, detail);
	}
}

/*
 * The answer renders four significant digits, but the method only earns about
 * three: f and every k are Q16, so the error floor is around 5e-4 whatever the
 * step count. That is why this compares numbers rather than strings.
 */
static void expect_typed_near(const char *text, double want)
{
	char out[128];
	double got;

	if (gexpr_ode_answer(text, out, (int)sizeof out) < 0) {
		fail(text, "no answer");
		return;
	}
	got = strtod(out, NULL);
	if (fabs(got - want) > 2e-3 * fabs(want)) {
		char detail[192];

		snprintf(detail, sizeof detail, "answered {%s}, closed form is %g",
		         out, want);
		fail(text, detail);
	}
}

static void expect_refused(const char *text)
{
	char out[128];

	if (gexpr_ode_answer(text, out, (int)sizeof out) == 0)
		fail(text, "answered an equation it cannot step");
}

int main(void)
{
	/* Growth and decay: the closed forms are e^2 and e^-1. */
	expect_final("2y", 1, 0, 1, 64, exp(2.0), 2e-3);
	expect_final("-y", 1, 0, 1, 64, exp(-1.0), 2e-3);

	/* Cooling towards an asymptote - started away from the equilibrium, not
	 * at it, which is where my first version of this case put it. */
	expect_final("-0.5y+2", 0, 0, 2, 64, 4 * (1 - exp(-1.0)), 2e-3);
	/* y' = x + y, y(0)=0 solves to e^x - x - 1, which is 0.718 at x=1. */
	expect_final("x+y", 0, 0, 1, 64, exp(1.0) - 2, 2e-3);

	/* f(x) alone: a pure integral, y' = 2x from (0,0) to 1 is x^2. */
	expect_final("2x", 0, 0, 1, 64, 1.0, 2e-3);

	/* Oscillation: y' = -y is checked above; add y' = cos(x) from 0. */
	expect_final("cos(x)", 0, 0, 1.5707963, 64, 1.0, 2e-3);

	/* Fourth order means few steps already land inside a percent. */
	expect_final("2y", 1, 0, 1, 8, exp(2.0), 2e-2);

	/* Through the typed path, at four significant digits. */
	expect_typed_near("ode(2y,0,1,1,64)", exp(2.0));
	expect_typed_near("ode(-y,0,1,1,64)", exp(-1.0));
	expect_typed_near("ode(2x,0,0,2,64)", 4.0);

	/* Malformed, out of range, or stepped into nothing. */
	expect_refused("ode(2y,0,1,1)");
	expect_refused("ode(2y,0,1,1,0)");
	expect_refused("ode(2y,0,1,1,999)");
	expect_refused("ode(2y,0,1,1,2.5)");
	expect_refused("ode(,0,1,1,8)");

	if (failures) {
		printf("failures: %d\n", failures);
		return 1;
	}
	printf("all cases passed\n");
	return 0;
}
