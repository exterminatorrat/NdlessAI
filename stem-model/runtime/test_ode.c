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

/*
 * The two methods are independent: RK4 steps numerically and the closed form is
 * derived. If they agree over an interval, the closed form is right - a wrong
 * one drifts away from the numerical solution immediately, which is why this is
 * the check worth having rather than a hand-written expected value.
 */
static void expect_closed_agrees(const char *text, double y0, double x0,
                                 double xn)
{
	char err[64];
	gexpr_t f, sol;
	q16_t ys[EXPR_MAX_ODE_STEPS + 1];
	double closed, numeric;

	if (gexpr_parse(text, &f, err, (int)sizeof err) < 0) {
		fail(text, err);
		return;
	}
	if (gexpr_ode_closed(&f, (q16_t)(x0 * Q16_ONE),
	                     (q16_t)(y0 * Q16_ONE), &sol) != GEXPR_DIFF_OK) {
		fail(text, "no closed form");
		return;
	}
	if (gexpr_rk4(&f, (q16_t)(x0 * Q16_ONE), (q16_t)(y0 * Q16_ONE),
	              (q16_t)(xn * Q16_ONE), 128, ys,
	              (int)(sizeof ys / sizeof ys[0])) != GEXPR_DIFF_OK) {
		fail(text, "rk4 refused a valid equation");
		return;
	}
	closed = gexpr_eval(&sol, (q16_t)(xn * Q16_ONE)) / (double)Q16_ONE;
	numeric = ys[128] / (double)Q16_ONE;
	if (fabs(closed - numeric) > 5e-3 * fabs(numeric)) {
		char detail[192];

		snprintf(detail, sizeof detail,
		         "closed form %g and RK4 %g disagree at x=%g",
		         closed, numeric, xn);
		fail(text, detail);
	}
}

static void expect_no_closed(const char *text)
{
	char err[64];
	gexpr_t f, sol;

	if (gexpr_parse(text, &f, err, (int)sizeof err) < 0) {
		fail(text, err);
		return;
	}
	if (gexpr_ode_closed(&f, 0, Q16_ONE, &sol) == GEXPR_DIFF_OK)
		fail(text, "solved an equation analytically it has no rule for");
}

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


/*
 * The printed closed form, as a student would read it. The text matters as
 * much as the value: an expression that is right and unreadable is a wrong
 * answer on this screen.
 */
static void expect_desolve(const char *call, const char *want)
{
	char got[128];

	if (gexpr_desolve_answer(call, got, (int)sizeof got) != 0) {
		fail(call, "desolve refused a solvable equation");
		return;
	}
	if (strcmp(got, want) != 0) {
		char detail[320];

		snprintf(detail, sizeof detail, "got \"%s\", wanted \"%s\"", got, want);
		fail(call, detail);
	}
}

static void expect_desolve_refused(const char *call)
{
	char got[128];

	if (gexpr_desolve_answer(call, got, (int)sizeof got) == 0)
		fail(call, "answered an equation it has no rule for");
}

/*
 * The slope field, checked against the evaluator rather than against a picture:
 * every segment has to point along the slope at its own cell. That is the part
 * a renderer cannot check for itself, and the part that would otherwise be
 * discovered on the device.
 */
static void expect_field(const char *text, double x0, double x1, double y0,
                         double y1, int cols, int rows)
{
	int16_t dx[EXPR_FIELD_MAX * EXPR_FIELD_MAX];
	int16_t dy[EXPR_FIELD_MAX * EXPR_FIELD_MAX];
	char err[64], detail[192];
	gexpr_t f;
	int r, c;

	if (gexpr_parse(text, &f, err, (int)sizeof err) < 0) {
		fail(text, err);
		return;
	}
	if (gexpr_slope_field(&f, (q16_t)(x0 * Q16_ONE), (q16_t)(x1 * Q16_ONE),
	                      (q16_t)(y0 * Q16_ONE), (q16_t)(y1 * Q16_ONE),
	                      cols, rows, dx, dy) != GEXPR_DIFF_OK) {
		fail(text, "slope field refused a valid window");
		return;
	}
	for (r = 0; r < rows; r++) {
		q16_t yv = (q16_t)((int64_t)(y1 * Q16_ONE) -
		                   (((int64_t)(y1 * Q16_ONE) - (int64_t)(y0 * Q16_ONE)) *
		                    (2 * r + 1)) / (2 * rows));
		for (c = 0; c < cols; c++) {
			q16_t xv = (q16_t)((int64_t)(x0 * Q16_ONE) +
			                   (((int64_t)(x1 * Q16_ONE) - (int64_t)(x0 * Q16_ONE)) *
			                    (2 * c + 1)) / (2 * cols));
			double m = gexpr_eval2(&f, xv, yv) / (double)Q16_ONE;
			int i = r * cols + c;
			double got;

			if (dx[i] == 0) {
				/* Almost vertical: the segment must be near its full
				 * half-length, not collapsed to a dot. */
				if (dy[i] < 6 * 64 - 2 || dy[i] > 6 * 64)
					fail(text, "vertical segment is not drawn");
				continue;
			}
			got = (double)dy[i] / (double)dx[i];
			if (fabs(got - m) > 0.05 * (1.0 + fabs(m)))
				snprintf(detail, sizeof detail,
				         "cell %d,%d: slope %g drawn as %g", r, c, m, got);
			else
				continue;
			fail(text, detail);
			return;
		}
	}
}

static void expect_field_refused(const char *text, double x0, double x1)
{
	char err[64];
	gexpr_t f;
	int16_t dx[4], dy[4];

	if (gexpr_parse(text, &f, err, (int)sizeof err) < 0) {
		fail(text, err);
		return;
	}
	if (gexpr_slope_field(&f, (q16_t)(x0 * Q16_ONE), (q16_t)(x1 * Q16_ONE),
	                      0, Q16_ONE, 2, 2, dx, dy) == GEXPR_DIFF_OK)
		fail(text, "accepted a window with no width");
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

	/* Closed form against the numerical solution: two independent methods. */
	expect_closed_agrees("2y", 1, 0, 1);
	expect_closed_agrees("-y", 1, 0, 2);
	expect_closed_agrees("-0.5y+2", 0, 0, 2);
	expect_closed_agrees("-0.5y+2", 1, 0, 3);
	expect_closed_agrees("3", 5, 0, 2);
	expect_closed_agrees("0.5y-0.01y^2", 10, 0, 4);
	expect_closed_agrees("0.2y-0.001y^2", 5, 0, 6);

	/* Separable with an x-dependent rate. RK4 knows nothing about how the
	 * closed form was derived, so agreement is two methods rather than one
	 * formula checked against itself. */
	expect_closed_agrees("x*y", 1, 0, 1);
	expect_closed_agrees("x*y", 2, 0, 2);
	expect_closed_agrees("-x*y", 1, 0, 2);
	expect_closed_agrees("x", 1, 0, 2);
	expect_closed_agrees("x/y", 1, 0, 1);
	expect_closed_agrees("x/y", 3, 0, 2);
	expect_closed_agrees("x/y", -1, 0, 1);
	expect_closed_agrees("2/y", 1, 0, 2);
	expect_closed_agrees("x*(1-y)", 0, 0, 1);
	expect_closed_agrees("x*y-0.01x*y^2", 5, 0, 2);

	/* Equations with no closed form here are refused rather than approximated
	 * silently - the numerical `ode` form still answers them. */
	expect_no_closed("x+y");
	expect_no_closed("y^3");
	expect_no_closed("sin(y)");
	expect_no_closed("x/y^2");
	expect_no_closed("y*sin(y)");
	expect_no_closed("sin(x*y)");

	/* The printed forms, because these are what a student reads. */
	expect_desolve("desolve(2y,0,1)", "exp(2x)");
	expect_desolve("desolve(-0.5y+2,0,0)", "-4exp(-0.5x)+4");
	expect_desolve("desolve(x*y,0,2)", "2exp(x^2/2)");
	expect_desolve("desolve(-x*y,0,1)", "exp(-x^2/2)");
	expect_desolve("desolve(x,0,1)", "1+x^2/2");
	expect_desolve("desolve(x/y,0,1)", "sqrt(1+x^2)");
	expect_desolve("desolve(x/y,0,-1)", "-sqrt(1+x^2)");
	expect_desolve("desolve(2/y,0,1)", "sqrt(1+4x)");
	expect_desolve_refused("desolve(x+y,0,1)");
	expect_desolve_refused("desolve(x/y^2,0,1)");
	expect_desolve_refused("desolve(y^3,0,1)");

	/* The slope field the graph mode will draw from, checked cell by cell. */
	expect_field("x-y", -2, 2, -2, 2, 8, 8);
	expect_field("y^2", -2, 2, -1, 1, 6, 6);
	expect_field("2y", -1, 1, -1, 1, 5, 4);
	expect_field_refused("x-y", 1, 1);

	if (failures) {
		printf("failures: %d\n", failures);
		return 1;
	}
	printf("all cases passed\n");
	return 0;
}
