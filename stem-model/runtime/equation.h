#ifndef EQUATION_H
#define EQUATION_H

#include "expr.h"
#include "solvers_subject.h"

/*
 * One equation solver, shared by every relation that has to solve for x.
 *
 * A relation becomes an adapter: it either builds the polynomial coefficients
 * straight from its arguments, or parses the two sides through expr.c and hands
 * them here. A new inverse family then costs one transform instead of a
 * hand-written function per direction.
 *
 * Three things this file can say about "lhs = rhs":
 *   - a root set, exactly or to four significant digits;
 *   - that it proved there is no solution, or that every x solves it;
 *   - that it has no rule for the shape, which a caller must never report to
 *     the student as "no solution".
 *
 * Nothing here uses float or double: coefficients are exact rationals and the
 * transform families go through the integer ln_9/exp_9/sqrt12 helpers in
 * solvers.c, the same ones the hand-written solvers use.
 */

#define EQ_MAX_DEGREE 8		/* largest polynomial degree the reducer keeps */
#define EQ_MAX_POWER  16	/* largest integer exponent it takes a root of */
#define EQ_MAX_ROOTS  2		/* degree 1 and 2 have at most two roots */

typedef struct {
	int degree;			/* -1 is the zero polynomial, else 0..EQ_MAX_DEGREE */
	rat c[EQ_MAX_DEGREE + 1];	/* c[0] + c[1]x + c[2]x^2 + ... */
} eq_poly_t;

typedef enum {
	EQ_SOLVED = 0,	/* root[0..n-1], ascending; exact unless approx is set */
	EQ_NONE,	/* proved: no real x solves it */
	EQ_ALL,		/* proved: every real x solves it (0 = 0) */
	EQ_UNSUPPORTED,	/* no rule for this shape: not the same as EQ_NONE */
	EQ_OVERFLOW	/* right shape, value outside the exact fixed-point domain */
} eq_status_t;

typedef struct {
	eq_status_t status;
	int n;
	int approx;		/* roots are fixed-point approximations, render as decimals */
	rat root[EQ_MAX_ROOTS];
} eq_result_t;

/*
 * Ordering on exact rationals, the one the solvers already sort roots with.
 * Exposed because both this engine and solvers_math.c sort root sets.
 */
int eq_rat_cmp(const rat *a, const rat *b);

/* Fill p from ascending coefficients, trimming the trailing zeros. */
void equation_poly_from(eq_poly_t *p, const rat *coef, int n);

/*
 * Reduce lhs - rhs to polynomial coefficients in x: +, -, products of sums,
 * division by a numeric constant, negation, and ^ with an integer exponent in
 * [0, EQ_MAX_POWER]; the degree is capped at EQ_MAX_DEGREE.
 * Returns 0 on success, 1 when the expression is not a polynomial within those
 * bounds (a bound is reported, never truncated), -1 when an exact intermediate
 * left the fixed-point domain.
 */
int equation_poly(const gexpr_t *lhs, const gexpr_t *rhs, eq_poly_t *p);

/* Exact solution of degree 0, 1 and 2; EQ_UNSUPPORTED above that. */
eq_status_t equation_solve_poly(const eq_poly_t *p, eq_result_t *out);

/*
 * Solve lhs = rhs for x: the polynomial cases above, plus a*b^(cx+d) = k by
 * logarithms and a*x^p = k by the p-th root. out->approx marks roots that came
 * from a logarithm or a root rather than from exact rational arithmetic.
 */
eq_status_t equation_solve(const gexpr_t *lhs, const gexpr_t *rhs,
                           eq_result_t *out);

#endif
