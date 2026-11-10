/*
 * Host tests for typed linear algebra: determinant, inverse, rank and square
 * systems, answered exactly with no model and no weights.
 *
 *   ./test_matrix
 */
#include <stdio.h>
#include <string.h>

#include "matrix.h"

static int failures;

static void expect(const char *text, const char *want)
{
	char got[128];

	if (matrix_typed_answer(text, got, sizeof got) != 0) {
		printf("FAIL %s -> not a matrix command, wanted %s\n", text, want);
		failures++;
		return;
	}
	if (strcmp(got, want)) {
		printf("FAIL %s -> %s, wanted %s\n", text, got, want);
		failures++;
	}
}

/* Recognised as a matrix request but not answerable: the caller shows the
 * message instead of a number, which is the point of the refusals below. */
static void expect_message(const char *text, const char *want)
{
	char got[128];

	if (matrix_typed_answer(text, got, sizeof got) != 0) {
		printf("FAIL %s -> not a matrix command, wanted message %s\n", text,
		       want);
		failures++;
		return;
	}
	if (strcmp(got, want)) {
		printf("FAIL %s -> %s, wanted message %s\n", text, got, want);
		failures++;
	}
}

/* Not a matrix request at all: the typed path must fall through to
 * arithmetic, so anything that is not exactly one of the four forms returns
 * -1 rather than a message. */
static void expect_not_matrix(const char *text)
{
	char got[128];

	if (matrix_typed_answer(text, got, sizeof got) == 0) {
		printf("FAIL %s -> %s, wanted no matrix answer\n", text, got);
		failures++;
	}
}

int main(void)
{
	/* Determinants: sign, exact zeros, and a 3x3 where the answer is not
	 * reachable by the 2x2 formula. */
	expect("det([1,2;3,4])", "-2");
	expect("det([2,0;0,3])", "6");
	expect("det([1,2;2,4])", "0");
	expect("det([0,1;1,0])", "-1");
	expect("det([1,2,3;4,5,6;7,8,10])", "-3");
	expect("det([2,0,0;0,3,0;0,0,4])", "24");

	/* Exact fractions on the way out. */
	expect("det([1,2;2,5])", "1");
	expect("solve([3,1;1,2],[1,1])", "x1 = 1/5, x2 = 2/5");

	/* Inverses, including a 3x3 identity (the augmented width is twice the
	 * matrix, so this is the case that catches a 4-column working buffer). */
	expect("inv([2,1;1,3])", "[3/5, -1/5; -1/5, 2/5]");
	expect("inv([1,0;0,1])", "[1, 0; 0, 1]");
	expect("inv([1,0,0;0,1,0;0,0,1])", "[1, 0, 0; 0, 1, 0; 0, 0, 1]");
	expect("inv([4,0;0,4])", "[1/4, 0; 0, 1/4]");
	expect_message("inv([1,2;2,4])", "no inverse: the matrix is singular");

	/* Rank, including a rectangular matrix and the zero matrix. */
	expect("rank([1,2;2,4])", "1");
	expect("rank([1,2;3,4])", "2");
	expect("rank([1,2,3;4,5,6])", "2");
	expect("rank([0,0;0,0])", "0");
	expect("rank([1,2,3;2,4,6])", "1");

	/* Systems: a unique solution, one written as a column vector, and the
	 * two singular outcomes, which must not be confused with each other. */
	expect("solve([2,1;1,3],[5,10])", "x1 = 1, x2 = 3");
	expect("solve([1,2;3,4],[5;6])", "x1 = -4, x2 = 9/2");
	expect("solve([1,0;0,1],[7,8])", "x1 = 7, x2 = 8");
	expect_message("solve([1,1;1,1],[1,1])",
	               "infinitely many solutions: the system is singular");
	expect_message("solve([1,1;1,1],[1,2])",
	               "no solution: the equations contradict");

	/* Shape refusals that are the student's mistake, not ours. */
	expect_message("det([1,2,3;4,5,6])", "determinant needs a square matrix");
	expect_message("det([1,2;3])", "malformed matrix: use [1,2;3,4]");
	expect_message("det([1,2;3,4,5])", "malformed matrix: use [1,2;3,4]");
	expect_message("det([1,1,1,1,1;1,1,1,1,1])",
	               "matrices are supported up to 4x4");
	expect_message("inv([1,2;3,4,5])", "malformed matrix: use [1,2;3,4]");
	expect_message("solve([1,2;3,4],[1,2,3])",
	               "solve: the right-hand side does not fit");

	/* A fraction inside the input is refused rather than read as "1". */
	expect_message("det([1/3,0;0,1])", "malformed matrix: use [1,2;3,4]");

	/* Spacing is tolerated where a student would type it. */
	expect("det( [1, 2; 3, 4] )", "-2");
	expect("DET([1,2;3,4])", "-2");

	/* Anything that is not exactly one of the four forms falls through to
	 * the arithmetic path instead of being half-answered here. */
	expect_not_matrix("");
	expect_not_matrix("2+2");
	expect_not_matrix("det([1,2;3,4])+1");
	expect_not_matrix("det [1,2;3,4]");
	expect_not_matrix("determinant([1,2;3,4])");
	expect_not_matrix("solve([1,2;3,4])");
	expect_message("det()", "malformed matrix: use [1,2;3,4]");

	if (failures) {
		printf("%d failure(s)\n", failures);
		return 1;
	}
	printf("linear algebra: all cases passed\n");
	return 0;
}
