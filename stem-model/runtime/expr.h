#ifndef EXPR_H
#define EXPR_H

#include <stdint.h>

/*
 * The expression engine: parse an infix expression in x and evaluate it in
 * Q16.16 fixed point. Shared by graphing and by typed answers.
 *
 * There is no FPU on the target, so nothing here uses float. Multiplication
 * goes through int64 and saturates, and a value with no real result (the root
 * of a negative, the logarithm of a non-positive number, an overflow)
 * saturates, which callers treat as "no value here".
 */

#define EXPR_MAX_NODES 48

typedef int32_t q16_t;

#define Q16_ONE ((q16_t)0x00010000)
#define Q16_MAX ((q16_t)0x7FFFFFFF)
#define Q16_MIN ((q16_t)(-0x7FFFFFFF))

enum {
	G_NUM, G_X, G_ADD, G_SUB, G_MUL, G_DIV, G_POW, G_SIN, G_COS, G_TAN, G_NEG,
	G_SQRT, G_ABS, G_LN, G_EXP
};

typedef struct {
	int16_t op;
	int16_t a;
	int16_t b;
	int32_t num;
} gnode_t;

typedef struct {
	gnode_t node[EXPR_MAX_NODES];
	int n;
	int root;
} gexpr_t;

q16_t gq_sat(int64_t v);
q16_t gq_mul(q16_t a, q16_t b);
q16_t gq_div(q16_t a, q16_t b);
q16_t gq_sin(q16_t x);
q16_t gq_cos(q16_t x);
q16_t gq_tan(q16_t x);

/*
 * Parses an expression in x. Supports + - * / ^, parentheses, decimal numbers,
 * the functions sin/cos/tan/sqrt/abs/ln/exp, the constants pi and e, and
 * implicit multiplication ("2x", "3sin(x)", "2pi"). Powers accept fractional
 * and large exponents. The whole text must be consumed.
 * Returns 0 on success; on failure returns -1 and writes a short message into
 * err.
 */
int gexpr_parse(const char *text, gexpr_t *e, char *err, int errlen);
q16_t gexpr_eval(const gexpr_t *e, q16_t x);

/* True when the expression depends on x, so it has no single value. */
int gexpr_has_x(const gexpr_t *e);

/*
 * Renders a value to four significant digits, the same precision the solvers
 * print. Returns 0 on success, -1 when the buffer is too small.
 */
int gexpr_format(q16_t v, char *out, int outlen);

enum {
	GEXPR_DIFF_OK = 0,
	GEXPR_DIFF_TOO_BIG = -1,
	GEXPR_DIFF_UNSUPPORTED = -2
};

/*
 * Writes d/dx f into out. The derivative shares f's nodes as a prefix and
 * appends only what the rules need, so the result is one tree that gexpr_eval
 * reads directly. Passing f itself as out appends in place and saves the copy,
 * at the cost of the source's node count.
 *
 * The simplifier runs while the tree is built rather than afterwards: numeric
 * children are folded and two numeric factors become one, and the identities
 * 0+a, a+0, a-0, 1*a, a*1, 0*a, a^1 and a^0 disappear as they would have been
 * created. That is what keeps a derivative inside EXPR_MAX_NODES instead of
 * doubling the tree at every product or quotient.
 *
 * Returns GEXPR_DIFF_OK, GEXPR_DIFF_TOO_BIG when the derivative does not fit,
 * or GEXPR_DIFF_UNSUPPORTED for an exponent that depends on x. On either
 * failure out->root is -1, so a half-built tree is never readable.
 */
int gexpr_derive(const gexpr_t *f, gexpr_t *out);

/*
 * Prints a tree in the syntax gexpr_parse accepts, with brackets where
 * precedence needs them and implicit multiplication where it reads better, so
 * a student sees "3x^2+2" rather than "3*x^2+2". A parse of the printed form
 * evaluates to the same values.
 * Returns 0 on success, -1 when the buffer is too small.
 */
int gexpr_unparse(const gexpr_t *e, char *out, int outlen);

/*
 * The typed-answer form of a derivative: "d/dx(<expr>)", "diff(<expr>)" or
 * "derivative(<expr>)". Matched on the raw text, because "d/dx" contains a
 * slash the expression parser would read as division. Writes the printed
 * derivative into out and returns 0, or returns -1 when the text is not one of
 * those forms or the derivative cannot be produced.
 */
int gexpr_derivative_answer(const char *text, char *out, int outlen);

enum {
	GEXPR_INT_OK = 0,
	GEXPR_INT_UNBOUNDED = -1
};

/*
 * The definite integral of f from a to b, in Q16.16, by adaptive Simpson that
 * only ever calls gexpr_eval.
 *
 * Three cases are answers rather than failures: a == b is 0, bounds given out
 * of order are the negated integral, and bounds that are the same Q16 value
 * are a zero-width interval. Everything else must be bounded to be reported.
 * That is the hard part at this precision: a sample that saturates means f has
 * no finite value there (or overflowed), a panel that cannot be made accurate
 * inside the depth, sample or range budget means the error is unknown, and a
 * result outside Q16 cannot be held. All of those return GEXPR_INT_UNBOUNDED
 * and leave out untouched, rather than printing a number the engine cannot
 * stand behind.
 */
int gexpr_integrate(const gexpr_t *f, q16_t a, q16_t b, q16_t *out);

/*
 * The typed-answer form of a definite integral: "integrate(<expr>, <a>, <b>)"
 * or "int(<expr>, <a>, <b>)", where the bounds are decimal literals (sign and
 * fraction allowed). Matched on the raw text, like the derivative form,
 * because the function name is not part of the expression grammar. The whole
 * text must be the call, so "integrate(x^2,0,3)+1" is refused. Writes the
 * answer to four significant digits and returns 0, or returns -1 when the
 * text is not one of those forms or the integral cannot be bounded.
 */
int gexpr_integral_answer(const char *text, char *out, int outlen);

#endif
