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

/* Revised 2026-11-16: cover chemistry quantities and numeric boundaries. */

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
	expect("CALL molarity 0.25 0.5", "0.25 mol in 0.5 L.", 1,
	       "complete decimal quantities");
	expect("CALL molarity 25 5", "0.25 mol in 0.5 L.", 0,
	       "decimal suffixes are not quantities");
	expect("CALL molarity 2 3", "2.5 mol in 3 L.", 0,
	       "decimal prefixes are not quantities");
	expect("CALL moles 2 44", "Find moles in CO2 with molar mass 44 g/mol.", 0,
	       "chemical subscripts are not quantities");
	expect("CALL moles 2 74", "Ca(OH)2 has molar mass 74 g/mol.", 0,
	       "parenthesized subscripts are not quantities");
	expect("CALL ph 3", "Find pH for 1e-3 mol/L.", 0,
	       "an exponent is not a quantity");
	expect("CALL ph 2", "Find pH for 2e-3 mol/L.", 0,
	       "a scientific mantissa is not the complete quantity");
	expect("CALL ph 0.005", "Find pH for 0.005.", 1,
	       "sentence punctuation may follow a quantity");
	expect("CALL quad_solve 1 -3 2", "Solve x^2-3x+2=0.", 1,
	       "adjacent polynomial signs and coefficients still match");
	expect("CALL log_eval 2 8", "Evaluate log_2(8).", 1,
	       "a subscripted logarithm base is still a quantity");
	expect("CALL arith_seq 12 -3 9",
	       "The sequence has a1 = 12 and d = -3. Find a_9 and S_9.", 1,
	       "subscripted term indices are still quantities");

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

	/* A sign flip is the failure seen on hardware: the model read
	 * "x^2 - 3x + 2" and emitted coefficients 1, 3, -2. Every digit is
	 * present in the text, so only a sign-aware check catches it. */
	expect("CALL quad_solve 1 -3 2", "Solve x^2 - 3x + 2 = 0 for x.", 1,
	       "correct signs");
	expect("CALL quad_solve 1 3 -2", "Solve x^2 - 3x + 2 = 0 for x.", 0,
	       "signs flipped");
	expect("CALL quad_solve 1 3 2", "Solve x^2 + 3x + 2 = 0 for x.", 1,
	       "all positive, no minus in text");
	expect("CALL quad_solve 1 -3 2", "Solve x^2 + 3x + 2 = 0 for x.", 0,
	       "negative argument but text has no minus");

	expect("CALL ohm_i 12 4", "A 12 V battery is across a 4 ohm resistor.", 1,
	       "plain two-argument relation");
	expect("CALL ohm_i 12 5", "A 12 V battery is across a 4 ohm resistor.", 0,
	       "wrong resistance");

	/* sig_figs is the first relation whose argument is a decimal, so the
	 * guard has to match the whole token rather than a digit run. */
	expect("CALL sig_figs 0.004567 2",
	       "Round 0.004567 to 2 significant figures.", 1,
	       "decimal argument present");
	expect("CALL sig_figs 0.004567 3",
	       "Round 0.004567 to 2 significant figures.", 0,
	       "wrong figure count");
	expect("CALL sig_figs 0.0046 2",
	       "Round 0.004567 to 2 significant figures.", 0,
	       "rounded value is not the given value");

	if (failures) {
		printf("%d guard case(s) failed\n", failures);
		return 1;
	}
	printf("guard: all cases passed\n");
	return 0;
}
