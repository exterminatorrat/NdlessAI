/*
 * Digit repair: the right relation with a number one keystroke off.
 *
 * Every case here is a real miss from the paraphrase corpus, so the test fails
 * if the repair stops helping with the failure that motivated it.
 */
#include <stdio.h>
#include <string.h>

#include "repair.h"

static int failures;

static void check(int cond, const char *what)
{
	if (!cond) {
		printf("FAIL %s\n", what);
		failures++;
	}
}

static void test_argument(void)
{
	char out[24];

	/* 0.1 became 0.2; 0.1 is the only number of that shape in the sentence. */
	check(repair_argument("0.2", "There is 0.1 mol of solute in 0.25 L.",
	                      out, (int)sizeof out) == 1 &&
	      strcmp(out, "0.1") == 0,
	      "a one-digit slip snaps back to the only candidate");

	/* Two equally close candidates means the difference is not in the digits. */
	check(repair_argument("0.2", "0.1 mol and 0.5 mol", out, (int)sizeof out) == 0,
	      "two equally close numbers is not a repair");

	/* A number already in the text is never touched. */
	check(repair_argument("0.25", "0.1 mol in 0.25 L", out,
	                      (int)sizeof out) == 0,
	      "a grounded number is not a repair");

	/* Different length is a different number, not a slip. */
	check(repair_argument("0.2", "the mass is 0.25 kg", out,
	                      (int)sizeof out) == 0,
	      "a different number of digits is not a slip");

	/* Coefficients are not numbers: "5x" must not be read as 5, so there is
	 * nothing of the right shape to repair 6 into. */
	check(repair_argument("6", "solve 5x = 30", out, (int)sizeof out) == 0,
	      "a coefficient is not a repair candidate");
}

static void test_call(void)
{
	char out[96];

	check(repair_call("CALL molarity 0.2 0.25",
	                  "There is 0.1 mol of solute in 0.25 L.", out,
	                  (int)sizeof out) == 1 &&
	      strcmp(out, "CALL molarity 0.1 0.25") == 0,
	      "the repaired call keeps the relation and the good argument");

	check(repair_call("CALL moles 117 58.5",
	                  "Molar mass 58.5 g/mol, 117 g on the spoon.", out,
	                  (int)sizeof out) == 0 &&
	      strcmp(out, "CALL moles 117 58.5") == 0,
	      "a grounded call is returned unchanged");

	/* Signs are polarity, not digits: -0.1 must not be repaired to 0.1. */
	check(repair_call("CALL x -0.1", "the value is 0.1", out,
	                  (int)sizeof out) == 0,
	      "a sign difference is not a digit slip");

	/* Two ungrounded numbers are not one keystroke. "exp_solve 2 29" against a
	 * sentence holding 6 and 20 is one character from "6 20" in both places,
	 * and snapping both stacked two guesses into a call nobody wrote. */
	check(repair_call("CALL exp_solve 2 29", "Work out 6 and 20.", out,
	                  (int)sizeof out) == 0 &&
	      strcmp(out, "CALL exp_solve 2 29") == 0,
	      "two repairs in one call are refused together");

	/* A repair may not repeat a number the call already has: "pythagoras 2 7"
	 * is one character from "7 7", which is not a triangle. */
	check(repair_call("CALL pythagoras 2 7", "legs 7 and 9", out,
	                  (int)sizeof out) == 0,
	      "a repair may not duplicate an argument");

	/* But a lone repair next to a grounded argument still goes through. */
	check(repair_call("CALL torque 65 6 145",
	                  "At 145 degrees to the arm, 6 m out, 55 N.", out,
	                  (int)sizeof out) == 1 &&
	      strcmp(out, "CALL torque 55 6 145") == 0,
	      "one repair beside grounded arguments is allowed");
}

int main(void)
{
	test_argument();
	test_call();
	if (failures) {
		printf("%d failure(s)\n", failures);
		return 1;
	}
	printf("repair: all checks passed\n");
	return 0;
}
