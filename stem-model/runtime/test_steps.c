/*
 * Checks steps_render: the block it formats, the fallback when a relation has
 * no template, and the truncation path.
 */
#include <stdio.h>
#include <string.h>

#include "steps.h"

static int failures;

static void expect(const char *what, const char *got, const char *want)
{
	if (strcmp(got, want) != 0) {
		printf("FAIL %s\n  got:  %s\n  want: %s\n", what, got, want);
		failures++;
	}
}

static void expect_code(const char *what, int got, int want)
{
	if (got != want) {
		printf("FAIL %s: got %d, want %d\n", what, got, want);
		failures++;
	}
}

int main(void)
{
	char out[256];
	const char *args2[] = {"25", "334"};
	const char *args_parallel[] = {"6", "3"};
	const char *args_quad[] = {"1", "-5", "6"};
	int rc;

	rc = steps_render("latent_heat", args2, 2, "8350 J", out, sizeof out);
	expect_code("latent_heat returns 1", rc, 1);
	expect("latent_heat block", out, "Q = m * L\n= 25 * 334\n= 8350 J");

	rc = steps_render("parallel_resistance", args_parallel, 2, "2 ohm",
	                  out, sizeof out);
	expect_code("parallel_resistance returns 1", rc, 1);
	expect("parallel_resistance block", out,
	       "R = 1/(1/R1 + 1/R2)\n= 1 / (1/6 + 1/3)\n= 2 ohm");

	/* A relation with no template shows nothing rather than a broken line. */
	rc = steps_render("quad_solve", args_quad, 3, "x=2, x=3", out, sizeof out);
	expect_code("quad_solve returns 0", rc, 0);
	expect("quad_solve writes an empty block", out, "");

	rc = steps_render("not_a_relation", args2, 2, "1", out, sizeof out);
	expect_code("unknown name returns 0", rc, 0);
	expect("unknown name writes an empty block", out, "");

	/*
	 * Too small to hold the block: the answer must still be terminated, and
	 * the caller is told it was truncated instead of printing half a line.
	 */
	rc = steps_render("latent_heat", args2, 2, "8350 J", out, 12);
	expect_code("short buffer returns -1", rc, -1);
	expect("short buffer stays terminated", out, "Q = m * L\n=");

	/* An out-of-range placeholder renders '?' rather than reading past args. */
	rc = steps_render("average", args2, 2, "17", out, sizeof out);
	expect_code("missing arguments return 1", rc, 1);
	expect("missing arguments render '?'",
	       out, "mean = (a + b + c + d)/4\n= (25 + 334 + ? + ?) / 4\n= 17");

	if (failures) {
		printf("failures: %d\n", failures);
		return 1;
	}
	printf("all cases passed\n");
	return 0;
}
