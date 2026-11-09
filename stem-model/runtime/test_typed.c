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
	/* The whole text has to be the call, and the integral has to exist:
	 * a divergent one is refused rather than approximated. */
	expect_refused("integrate(x^2,0,3)+1");
	expect_refused("integrate(x^2,0)");
	expect_refused("integrate(x,0,x)");
	expect_refused("integrate(1/x,-1,1)");
	expect_refused("");

	if (failures) {
		printf("%d failure(s)\n", failures);
		return 1;
	}
	printf("typed answers: all cases passed\n");
	return 0;
}
