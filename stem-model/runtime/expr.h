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

#endif
