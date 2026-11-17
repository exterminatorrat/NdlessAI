/*
 * Host tests for typed answers: text that is already an expression, answered
 * with no model, no weights and no inference.
 *
 *   ./test_typed
 */
#include <stdio.h>
#include <string.h>

#include "pipeline.h"

static int failures;

static void expect(const char *text, const char *want)
{
	char got[64];

	if (pipeline_typed_answer(text, got, sizeof got) != PIPELINE_OK) {
		printf("FAIL %s -> no answer, wanted %s\n", text, want);
		failures++;
		return;
	}
	if (strcmp(got, want)) {
		printf("FAIL %s -> %s, wanted %s\n", text, got, want);
		failures++;
	}
}

static void expect_refused(const char *text)
{
	char got[64];

	if (pipeline_typed_answer(text, got, sizeof got) == PIPELINE_OK) {
		printf("FAIL %s -> %s, wanted no typed answer\n", text, got);
		failures++;
	}
}

/*
 * Where the offline path sends text it cannot answer exactly.
 *
 * The exact engine has a fixed node pool, so long arithmetic fails to parse.
 * It must not then reach the decoder: the decoder reads it as a word problem
 * and answers with invented numbers, which is worse than saying it cannot.
 */
static int sum_of_ones(char *out, int terms)
{
	int len = 0;

	out[0] = '\0';
	for (int i = 0; i < terms; i++)
		len += sprintf(out + len, "%s1", i ? "+" : "");
	return len;
}

static void check_too_long_is_not_a_word_problem(void)
{
	char answer[64], call[64], sum[300];

	/* 65 terms is 129 characters: past the pool, and not a word problem. The
	 * code is its own so the screen can say which of the two happened. */
	sum_of_ones(sum, 65);
	if (pipeline_answer_offline(sum, answer, (int)sizeof answer, call,
	                            (int)sizeof call) != 3) {
		printf("FAIL a 65-term sum reaches the decoder\n");
		failures++;
	}
	/* 64 terms is 127 characters, the last length the pool holds. */
	sum_of_ones(sum, 64);
	if (pipeline_answer_offline(sum, answer, (int)sizeof answer, call,
	                            (int)sizeof call) != 0 || strcmp(answer, "64")) {
		printf("FAIL a 64-term sum is not answered exactly: %s\n", answer);
		failures++;
	}
	/* Prose that merely contains arithmetic still belongs to the decoder. */
	if (pipeline_answer_offline("What is 2+2?", answer, (int)sizeof answer,
	                            call, (int)sizeof call) != 1) {
		printf("FAIL a question about arithmetic is refused\n");
		failures++;
	}
	if (pipeline_answer_offline("2+2=?", answer, (int)sizeof answer, call,
	                            (int)sizeof call) != 1) {
		printf("FAIL arithmetic with a question mark is refused\n");
		failures++;
	}
	if (pipeline_answer_offline("2+2", answer, (int)sizeof answer, call,
	                            (int)sizeof call) != 0 || strcmp(answer, "4")) {
		printf("FAIL short arithmetic stopped being exact: %s\n", answer);
		failures++;
	}
}

int main(void)
{
	expect("2+2", "4");
	expect("2^10", "1024");
	expect("3.14159*2", "6.283");
	expect("sqrt(50)", "7.071");
	expect("ln(2)", "0.6931");
	expect("exp(0)", "1");
	expect("e^2", "7.389");
	expect("1/3", "0.3333");
	expect("abs(-7)", "7");
	expect("limit(sin(x)/x,0)", "1");
	expect("tangent(x^2,3)", "y = 6x - 9");
	expect("avgrate(x^2,1,3)", "4");
	expect("riemann(x^2,0,1,1000)", "0.3333");
	expect("avgvalue(x^2,0,3)", "3");
	expect("arclen(x,0,1)", "1.414");
	expect("sin(0)", "0");
	expect("pi", "3.142");
	expect("2pi", "6.283");
	expect("2^0.5", "1.414");

	/* A derivative of an expression is answered as an expression. */
	expect("d/dx(x^3+2x)", "3x^2+2");
	expect("diff(x^2)", "2x");
	expect("derivative(sin(x))", "cos(x)");
	expect("d/dx(5)", "0");

	/* A definite integral of an expression is answered with a number, with
	 * no model, no weights and no inference. */
	expect("integrate(x^2,0,3)", "9");
	expect("integrate(x^2, 0, 3)", "9");
	expect("int(x^2,0,3)", "9");
	expect("int (x^2 , 0 , 3)", "9");
	expect("integrate(1/x,1,4)", "1.386");
	expect("integrate(sin(x),0,3.14159)", "2");
	expect("int(sqrt(x),0,1)", "0.6667");
	expect("integrate(abs(x),-2,3)", "6.5");
	expect("integrate(x^2,3,0)", "-9");
	/* A bound is a constant expression in Q16, so pi is 3.14159 here. */
	expect("integrate(x,0,pi)", "4.935");
	expect("integrate(sin(x),0,pi)", "2");
	expect("integrate(x,0,2*pi)", "19.74");

	/* x has no single value, and a word problem is not an expression. */
	expect_refused("x^2+1");
	expect_refused("A 5 kg block slides down a ramp");
	expect_refused("d/dx(2^x)");
	expect_refused("d/dx(x^");
	/* Linear algebra is answered before the expression parser sees the
	 * text: brackets, ';' and ',' have no arithmetic meaning. */
	expect("det([1,2;3,4])", "-2");
	expect("inv([2,1;1,3])", "[3/5, -1/5; -1/5, 2/5]");
	expect("solve([2,1;1,3],[5,10])", "x1 = 1, x2 = 3");
	expect("rank([1,2;2,4])", "1");

	/* The whole text has to be the call, and the integral has to exist:
	 * a divergent one is refused rather than approximated. */
	expect_refused("integrate(x^2,0,3)+1");
	expect_refused("integrate(x^2,0)");
	expect_refused("integrate(x,0,x)");
	expect_refused("integrate(1/x,-1,1)");
	expect_refused("");

	check_too_long_is_not_a_word_problem();

	if (failures) {
		printf("%d failure(s)\n", failures);
		return 1;
	}
	printf("typed answers: all cases passed\n");
	return 0;
}
