/*
 * Regression tests for the grounding guard.
 *
 * The first version never terminated the end of each word, so it compared the
 * whole remaining string and rejected almost every correct call. That cost 20
 * percentage points of delivered answers before it was caught, so the cases
 * below pin the behaviour down.
 */
#include <stdio.h>
#include <string.h>

#include "pipeline.h"

static int failures;

static void expect(const char *call, const char *question, int want, const char *why)
{
	int got = pipeline_args_grounded(call, question);

	if (got != want) {
		printf("FAIL %s: %s\n  call: %s\n  question: %s\n",
		       why, got ? "accepted" : "rejected", call, question);
		failures++;
	}
}

int main(void)
{
	const char *dilution =
	    "What is the molarity after diluting 50 mL of 4 M solution "
	    "to a final volume of 250 mL?";
	const char *deriv =
	    "Evaluate the derivative of f(x) = x^3 + 3x^2 - 2x - 3 at x = 2.";
	const char *integr =
	    "Compute the definite integral of 3x^3 + 2x^2 + 2x - 1 "
	    "from x = 1 to x = 4.";

	expect("CALL dilution 4 50 250", dilution, 1, "all arguments present");
	expect("CALL dilution 4 50 77", dilution, 0, "hallucinated 77");
	expect("CALL dilution 4 50 -9", dilution, 0, "hallucinated -9");

	expect("CALL deriv_poly3 1 3 -2 -3 2", deriv, 1,
	       "implicit leading coefficient 1");
	expect("CALL deriv_poly3 1 3 -2 -3 7", deriv, 0,
	       "hallucinated evaluation point");

	expect("CALL integr_poly3 3 2 2 -1 1 4", integr, 1,
	       "negative coefficient written as '- 1' in the text");
	expect("CALL integr_poly3 3 2 2 -1 1 9", integr, 0,
	       "hallucinated upper limit");

	expect("CALL quad_solve 1 1 0", "Solve x^2 + x = 0 for x.", 1,
	       "implicit coefficient and zero constant");
	expect("CALL ohm_i 12 4", "A 12 V battery is across a 4 ohm resistor.", 1,
	       "plain two-argument relation");
	expect("CALL ohm_i 12 5", "A 12 V battery is across a 4 ohm resistor.", 0,
	       "wrong resistance");

	if (failures) {
		printf("%d guard case(s) failed\n", failures);
		return 1;
	}
	printf("guard: all cases passed\n");
	return 0;
}
