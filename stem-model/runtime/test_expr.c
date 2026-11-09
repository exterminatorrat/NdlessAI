/*
 * Host tests for symbolic differentiation and definite integration: the rules,
 * the simplifier, the printed forms and the typed-answer paths.
 *
 * A table of expected strings only pins down the cases the author thought of,
 * so the real check is numeric. For every expression, hand-picked or drawn
 * from a deterministic generator, the symbolic derivative is compared with a
 * central difference of the source at twenty interior points, and the printed
 * derivative is re-parsed and compared with the tree it came from. Fixed point
 * is noisy at a finite step, so the tolerance below is the worst disagreement
 * actually measured on this tree, not a number chosen to look tidy.
 *
 *   ./test_expr
 */
#include <stdio.h>
#include <string.h>

#include "expr.h"

#define NDIM 20
#define XSTEP (Q16_ONE / 16)	/* sample points are odd sixteenths, so none
				 * lands on 0 or 1, where ln, 1/x and 1/(x-1)
				 * have no value */
#define HSTEP (Q16_ONE / 128)	/* the difference step, also exact in Q16 */

/*
 * Disagreement is measured in permille of max(1, |value|): relative for slopes
 * above one, and absolute in units of one below that. The measured worst on
 * this tree is 15 permille, at a generated expression whose slope near x=2.9 is
 * 0.003, where evaluating the source twice a step apart costs more precision
 * than the step carries. The assertion sits at roughly twice that floor, which
 * is still far below the error a wrong rule makes: a missing chain factor is
 * 1000 permille or more.
 */
#define TOL_PERMILLE 25

static int failures;

static int worst_fd;		/* worst disagreement with the finite difference */
static int worst_print;		/* worst disagreement after print and re-parse */
static int too_big_to_print;	/* printed forms past the parser's node cap */
static char worst_fd_where[128];
static char worst_print_where[128];

/* |a - b| as a permille of max(1, |a|), which keeps a small derivative from
 * looking perfect and a large one from looking broken. */
static int permille(q16_t a, q16_t b)
{
	int64_t d = (int64_t)a - b;
	int64_t s = a < 0 ? -(int64_t)a : a;

	if (d < 0)
		d = -d;
	if (s < Q16_ONE)
		s = Q16_ONE;
	return (int)(d * 1000 / s);
}

static int saturated(q16_t v)
{
	return v == Q16_MAX || v == Q16_MIN;
}

static q16_t sample_x(int k)
{
	return (q16_t)((9 + 2 * k) * XSTEP);
}

/* The slope of f between x-h and x+h, in Q16, without a second rounding. */
static q16_t central(const gexpr_t *f, q16_t x, q16_t h)
{
	q16_t hi = gexpr_eval(f, gq_sat((int64_t)x + h));
	q16_t lo = gexpr_eval(f, gq_sat((int64_t)x - h));

	return gq_sat((((int64_t)hi - lo) << 16) / (2 * (int64_t)h));
}

/*
 * How many nodes the printed text needs. Printing cannot share, so a subtree a
 * rule reused counts once per appearance, and the printed form of a large
 * derivative can need more nodes than the tree itself holds. Beyond
 * EXPR_MAX_NODES the parser cannot read the text back at all, which is a limit
 * of the expression format, not of the derivative.
 */
static int64_t expand_size(const gexpr_t *e, int i, int64_t *memo)
{
	const gnode_t *n = &e->node[i];
	int64_t total = 1;

	if (memo[i] >= 0)
		return memo[i];
	switch (n->op) {
	case G_NUM:
	case G_X:
		break;
	case G_ADD:
	case G_SUB:
	case G_MUL:
	case G_DIV:
	case G_POW:
		total += expand_size(e, n->a, memo) + expand_size(e, n->b, memo);
		break;
	default:
		total += expand_size(e, n->a, memo);
		break;
	}
	if (total > 1000000)
		total = 1000000;
	memo[i] = total;
	return total;
}

/*
 * Checks one derivative: the printed form against the tree, and the tree
 * against the source's slope when slope is set. Returns 1 when everything
 * agrees, so a failing expression reports once instead of once per point.
 */
static int check(const char *text, const gexpr_t *f, const gexpr_t *d, int slope)
{
	char shown[320];
	char err[48];
	gexpr_t back;
	int64_t memo[EXPR_MAX_NODES];
	int printable = 1;

	if (gexpr_unparse(d, shown, sizeof shown) < 0) {
		printf("FAIL %s: printed form does not fit\n", text);
		failures++;
		return 0;
	}
	for (int i = 0; i < EXPR_MAX_NODES; i++)
		memo[i] = -1;
	if (expand_size(d, d->root, memo) > EXPR_MAX_NODES) {
		printable = 0;
		too_big_to_print++;
	} else if (gexpr_parse(shown, &back, err, sizeof err) < 0) {
		printf("FAIL %s: printed %s does not parse: %s\n", text, shown, err);
		failures++;
		return 0;
	}
	for (int k = 0; k < NDIM; k++) {
		q16_t x = sample_x(k);
		q16_t sym = gexpr_eval(d, x);
		int p;

		if (saturated(sym)) {
			printf("FAIL %s: %s has no value at x=%d/16\n", text,
			       shown, 9 + 2 * k);
			failures++;
			return 0;
		}
		if (slope) {
			q16_t num = central(f, x, HSTEP);

			if (saturated(num)) {
				printf("FAIL %s: no slope at x=%d/16\n", text,
				       9 + 2 * k);
				failures++;
				return 0;
			}
			p = permille(sym, num);
			if (p > worst_fd) {
				worst_fd = p;
				snprintf(worst_fd_where, sizeof worst_fd_where,
				         "%s at x=%d/16", text, 9 + 2 * k);
			}
			if (p > TOL_PERMILLE) {
				printf("FAIL %s: d/dx is %d, the slope is %d, at"
				       " x=%d/16 (%d permille)\n", text, (int)sym,
				       (int)num, 9 + 2 * k, p);
				failures++;
				return 0;
			}
		}
		if (!printable)
			continue;
		p = permille(sym, gexpr_eval(&back, x));
		if (p > worst_print) {
			worst_print = p;
			snprintf(worst_print_where, sizeof worst_print_where,
			         "%s at x=%d/16", text, 9 + 2 * k);
		}
		if (p > TOL_PERMILLE) {
			printf("FAIL %s: printed %s reads back differently at"
			       " x=%d/16 (%d permille)\n", text, shown, 9 + 2 * k,
			       p);
			failures++;
			return 0;
		}
	}
	return 1;
}

/* ---- hand-picked expressions ---- */

/*
 * slope is 0 for tan only: the sample interval covers pi/2, where the slope is
 * real but no central difference at 1/128 can resolve it. tan is still pinned
 * by its printed form and by the round-trip check.
 */
static const struct {
	const char *text;
	const char *want;
	int slope;
} cases[] = {
	{ "x^3+2x", "3x^2+2", 1 },
	{ "5", "0", 1 },
	{ "x", "1", 1 },
	{ "x^2-4x+7", "2x-4", 1 },
	{ "sin(x)", "cos(x)", 1 },
	{ "cos(x)", "-sin(x)", 1 },
	{ "tan(x)", "1/cos(x)^2", 0 },
	{ "exp(3x)", "3exp(3x)", 1 },
	{ "ln(x)", "1/x", 1 },
	{ "sqrt(x)", "1/(2sqrt(x))", 1 },
	{ "1/x", "-1/x^2", 1 },
	{ "x*sin(x)", "sin(x)+xcos(x)", 1 },
	{ "(x^2+1)/(x-1)", "(2x(x-1)-(x^2+1))/(x-1)^2", 1 },
	{ "sin(x)*cos(x)", "cos(x)cos(x)-sin(x)sin(x)", 1 },
	{ "ln(x^2+1)", "2x/(x^2+1)", 1 },
	{ "abs(x)", "x/abs(x)", 1 },
	{ "pi*x^2", "6.283173x", 1 },
	{ "2*x^3-5*x^2+3*x-1", "6x^2-10x+3", 1 },
	{ "x^4", "4x^3", 1 },
	{ "sin(x^3)", "cos(x^3)(3x^2)", 1 },
};

static void test_cases(void)
{
	for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
		gexpr_t f, d;
		char shown[320];
		char err[48];

		if (gexpr_parse(cases[i].text, &f, err, sizeof err) < 0) {
			printf("FAIL %s does not parse: %s\n", cases[i].text, err);
			failures++;
			continue;
		}
		if (gexpr_derive(&f, &d) != GEXPR_DIFF_OK ||
		    gexpr_unparse(&d, shown, sizeof shown) < 0) {
			printf("FAIL %s: no printed derivative\n", cases[i].text);
			failures++;
			continue;
		}
		if (strcmp(shown, cases[i].want)) {
			printf("FAIL %s -> %s, wanted %s\n", cases[i].text, shown,
			       cases[i].want);
			failures++;
		}
		check(cases[i].text, &f, &d, cases[i].slope);
	}
}

/*
 * |x| has no derivative at 0, and the printed form says so without dividing by
 * zero: it is the sign of x on both sides, which is what the tree evaluates to
 * away from the origin.
 */
static void test_abs_sides(void)
{
	gexpr_t f, d;
	char err[48], shown[64];

	if (gexpr_parse("abs(x)", &f, err, sizeof err) < 0 ||
	    gexpr_derive(&f, &d) != GEXPR_DIFF_OK ||
	    gexpr_unparse(&d, shown, sizeof shown) < 0) {
		printf("FAIL abs(x): no printed derivative\n");
		failures++;
		return;
	}
	if (gexpr_eval(&d, 2 * Q16_ONE) != Q16_ONE ||
	    gexpr_eval(&d, -2 * Q16_ONE) != -Q16_ONE) {
		printf("FAIL %s at x=+-2: %d and %d, wanted 1 and -1\n", shown,
		       (int)gexpr_eval(&d, 2 * Q16_ONE),
		       (int)gexpr_eval(&d, -2 * Q16_ONE));
		failures++;
	}
}

/* Deriving in place appends to the source tree, which is what a caller that no
 * longer needs the source does to save a copy. */
static void test_in_place(void)
{
	gexpr_t f;
	char err[48], shown[64];

	if (gexpr_parse("x^3+2x", &f, err, sizeof err) < 0 ||
	    gexpr_derive(&f, &f) != GEXPR_DIFF_OK ||
	    gexpr_unparse(&f, shown, sizeof shown) < 0 ||
	    strcmp(shown, "3x^2+2")) {
		printf("FAIL in place: wanted 3x^2+2\n");
		failures++;
	}
}

/* ---- generated expressions ---- */

static uint32_t rng_state = 1;

static uint32_t rnd(void)
{
	rng_state = rng_state * 1664525u + 1013904223u;
	return rng_state >> 16;
}

static int gen_room;		/* cleared when a generation overflows */

static void gen_str(char *b, int *len, int cap, const char *s)
{
	while (*s) {
		if (*len + 1 >= cap) {
			gen_room = 0;
			return;
		}
		b[(*len)++] = *s++;
	}
}

/*
 * The shapes are chosen so every value stays finite and smooth on the sample
 * interval: a division by a square plus one, a logarithm and a root of the
 * same, and an exponential of a negated square, which is bounded by one.
 */
static const struct {
	const char *open;
	const char *join;	/* between two children; 0 for one child */
	const char *close;
} shapes[] = {
	{ "(", "+", ")" },
	{ "(", "-", ")" },
	{ "(", "*", ")" },
	{ "(", ")/((", ")^2+1)" },
	{ "(", 0, ")^2" },
	{ "(", 0, ")^3" },
	{ "sin(", 0, ")" },
	{ "cos(", 0, ")" },
	{ "exp(-(", 0, ")^2)" },
	{ "ln((", 0, ")^2+1)" },
	{ "sqrt((", 0, ")^2+1)" },
};

static void gen_rec(char *b, int *len, int cap, int depth)
{
	const char *const atoms[] = { "x", "x", "2", "3", "4" };
	size_t s;

	if (!gen_room)
		return;
	if (depth <= 0 || rnd() % 4 == 0) {
		gen_str(b, len, cap, atoms[rnd() % 5]);
		return;
	}
	s = rnd() % (sizeof shapes / sizeof shapes[0]);
	gen_str(b, len, cap, shapes[s].open);
	gen_rec(b, len, cap, depth - 1);
	if (shapes[s].join) {
		gen_str(b, len, cap, shapes[s].join);
		gen_rec(b, len, cap, depth - 1);
	}
	gen_str(b, len, cap, shapes[s].close);
}

static int gen_text(char *b, int cap)
{
	int len = 0;

	gen_room = 1;
	gen_rec(b, &len, cap, 3);
	if (!gen_room)
		return -1;
	b[len] = 0;
	return 0;
}

static int source_saturates(const gexpr_t *f)
{
	for (int k = 0; k < NDIM; k++)
		if (saturated(gexpr_eval(f, sample_x(k))))
			return 1;
	return 0;
}

#define NGEN 30

static void test_generated(void)
{
	int made = 0, tries = 0, rejected = 0;

	while (made < NGEN && tries < 500) {
		char text[320];
		gexpr_t f, d;
		char err[48];

		tries++;
		if (gen_text(text, sizeof text) < 0 ||
		    gexpr_parse(text, &f, err, sizeof err) < 0 ||
		    !gexpr_has_x(&f) || source_saturates(&f)) {
			/* A saturated source carries no slope information, so
			 * that draw says nothing about the derivative. */
			rejected++;
			continue;
		}
		if (gexpr_derive(&f, &d) != GEXPR_DIFF_OK) {
			printf("FAIL generated %s: derivative does not fit\n", text);
			failures++;
		} else {
			check(text, &f, &d, 1);
		}
		made++;
	}
	if (made < NGEN) {
		printf("FAIL generated: only %d of %d expressions were usable\n",
		       made, NGEN);
		failures++;
	}
	printf("generated: %d expressions from %d draws, %d rejected\n", made,
	       tries, rejected);
}

/* ---- definite integrals ---- */

/*
 * Expected values are the closed forms evaluated by sympy at the Q16 bounds the
 * engine actually receives, rounded to the nearest Q16 step. That way the table
 * measures the integrator and not the bounds' own quantisation: a student who
 * types integrate(1/x,0.001,1) is asking about 65/65536, because that is the
 * nearest value Q16 holds.
 *
 * The error is reported in millionths of max(1, |value|), which is the
 * derivative checks' permille with three more digits: integration is accurate
 * enough that a whole permille would round most of the table to zero. The worst
 * disagreement measured over this table is 107 ppm, at sin(x) over [0,20] -
 * twenty is more than six periods, so the grid is dense and the residual is the
 * samples' own error. The assertion sits at 1000 ppm, about nine times that,
 * which is still far below what a wrong rule makes: a dropped panel or a sign
 * flip is hundreds of thousands of ppm. Every case is checked against a value
 * derived independently, so the table pins the number and not a shape.
 */
#define INT_TOL_PPM 1000

static int worst_int;
static char worst_int_where[128];

/* |a - b| as millionths of max(1, |a|), the integral counterpart of permille(). */
static int ppm(q16_t a, q16_t b)
{
	int64_t d = (int64_t)a - b;
	int64_t s = a < 0 ? -(int64_t)a : a;

	if (d < 0)
		d = -d;
	if (s < Q16_ONE)
		s = Q16_ONE;
	return (int)(d * 1000000 / s);
}

static const struct {
	const char *text;
	q16_t a;
	q16_t b;
	q16_t want;
} integrals[] = {
	{ "x^2", 0, 3 * Q16_ONE, 589824 },
	{ "x^3+2x", 0, 2 * Q16_ONE, 524288 },
	{ "2x^3-5x^2+3x-1", -Q16_ONE, 2 * Q16_ONE, -393216 },
	{ "3x^2-4x+1", -2 * Q16_ONE, 2 * Q16_ONE, 1310720 },
	{ "x^10", -Q16_ONE, Q16_ONE, 11916 },
	{ "sin(x)", 0, 205887, 131072 },
	{ "cos(x)", 0, Q16_ONE, 55147 },
	{ "sin(x)^2", 0, 3 * Q16_ONE, 102882 },
	{ "sin(3x)", 0, 2 * Q16_ONE, 870 },
	{ "tan(x)", 0, Q16_ONE, 40346 },
	{ "exp(x)", 0, Q16_ONE, 112609 },
	{ "exp(-x^2)", 0, Q16_ONE, 48944 },
	{ "exp(-x)", 0, 10 * Q16_ONE, 65533 },
	{ "exp(x)", 0, 5 * Q16_ONE, 9660869 },
	{ "ln(x)", Q16_ONE, 3 * Q16_ONE, 84924 },
	{ "ln(x^2+1)", 0, Q16_ONE, 17298 },
	{ "1/x", Q16_ONE, 4 * Q16_ONE, 90852 },
	{ "1/(1+x)", 0, Q16_ONE, 45426 },
	{ "1/(1+x)", 0, 100 * Q16_ONE, 302457 },
	/* 1/x with the lower bound at 65/65536: a steep endpoint that is not a
	 * singularity, the case a fixed grid is worst at. */
	{ "1/x", 65, Q16_ONE, 453245 },
	{ "sqrt(x)", 0, Q16_ONE, 43691 },
	{ "sqrt(x)", Q16_ONE, 4 * Q16_ONE, 305835 },
	{ "sqrt(1-x^2)", 0, Q16_ONE, 51472 },
	/* A kink the panel grid does not land on. */
	{ "abs(x-0.5)", 0, Q16_ONE, 16384 },
	{ "abs(x-0.3)", 0, Q16_ONE, 19005 },
	{ "abs(x)", -2 * Q16_ONE, 3 * Q16_ONE, 425984 },
	{ "1/(1+x^2)", 0, 4 * Q16_ONE, 86889 },
	{ "1/(1+x^2)", 0, 10 * Q16_ONE, 96412 },
	/* A singularity just outside the interval. */
	{ "1/(x-1)", 0, Q16_ONE / 2, -45426 },
	{ "1/(x-1)", 26214, 58982, -117421 },
	{ "x*sin(x)", 0, 205887, 205887 },
	{ "x^2*exp(-x)", 0, 2 * Q16_ONE, 42379 },
	/* Nearly constant curves, and twenty panels of one. */
	{ "1", 0, 5 * Q16_ONE, 327680 },
	{ "0.5", -2 * Q16_ONE, 2 * Q16_ONE, 131072 },
	{ "1+0.0625x", 0, 8 * Q16_ONE, 655360 },
	{ "sin(x)", 0, 20 * Q16_ONE, 38792 },
};

static void test_integrals(void)
{
	for (size_t i = 0; i < sizeof integrals / sizeof integrals[0]; i++) {
		gexpr_t f;
		char err[48];
		q16_t got = 0;
		int p;

		if (gexpr_parse(integrals[i].text, &f, err, sizeof err) < 0) {
			printf("FAIL %s does not parse: %s\n", integrals[i].text,
			       err);
			failures++;
			continue;
		}
		if (gexpr_integrate(&f, integrals[i].a, integrals[i].b,
		                    &got) != GEXPR_INT_OK) {
			printf("FAIL %s over [%ld,%ld]: refused\n",
			       integrals[i].text, (long)integrals[i].a,
			       (long)integrals[i].b);
			failures++;
			continue;
		}
		p = ppm(got, integrals[i].want);
		if (p > worst_int) {
			worst_int = p;
			snprintf(worst_int_where, sizeof worst_int_where,
			         "%s over [%ld,%ld]", integrals[i].text,
			         (long)integrals[i].a, (long)integrals[i].b);
		}
		if (p > INT_TOL_PPM) {
			printf("FAIL %s over [%ld,%ld]: %d, wanted %d (%d ppm)\n",
			       integrals[i].text,
			       (long)integrals[i].a, (long)integrals[i].b,
			       (int)got, (int)integrals[i].want, p);
			failures++;
		}
	}
}

/* ---- integrals that must fail, and the three cases that are answers ---- */

static void expect_refused_integral(const char *text, q16_t a, q16_t b)
{
	gexpr_t f;
	char err[48];
	q16_t got = 12345;

	if (gexpr_parse(text, &f, err, sizeof err) < 0) {
		printf("FAIL %s does not parse: %s\n", text, err);
		failures++;
		return;
	}
	if (gexpr_integrate(&f, a, b, &got) != GEXPR_INT_UNBOUNDED) {
		printf("FAIL %s over [%ld,%ld]: answered %d\n", text, (long)a,
		       (long)b, (int)got);
		failures++;
		return;
	}
	/* A refused integral must not leave half an answer behind. */
	if (got != 12345) {
		printf("FAIL %s: refused but wrote %d\n", text, (int)got);
		failures++;
	}
}

static void test_integral_edges(void)
{
	gexpr_t f, empty = { .n = 0, .root = -1 };
	char err[48];
	q16_t up = 0, down = 0, zero = 777;

	if (gexpr_parse("x^2+1", &f, err, sizeof err) < 0) {
		printf("FAIL setup for the integral edge cases\n");
		failures++;
		return;
	}

	/* a == b is zero, and a zero-width interval is not an error even where
	 * the function has no value there: integrating over a point adds
	 * nothing. */
	if (gexpr_integrate(&f, 2 * Q16_ONE, 2 * Q16_ONE, &zero) !=
	        GEXPR_INT_OK || zero != 0) {
		printf("FAIL a == b is not 0\n");
		failures++;
	}
	if (gexpr_parse("1/x", &f, err, sizeof err) < 0 ||
	    gexpr_integrate(&f, 0, 0, &zero) != GEXPR_INT_OK || zero != 0) {
		printf("FAIL 1/x over [0,0] is not 0\n");
		failures++;
	}

	/* Out of order is the negated integral, not a refusal and not zero. */
	if (gexpr_parse("x^2+1", &f, err, sizeof err) < 0 ||
	    gexpr_integrate(&f, 0, 2 * Q16_ONE, &up) != GEXPR_INT_OK ||
	    gexpr_integrate(&f, 2 * Q16_ONE, 0, &down) != GEXPR_INT_OK ||
	    (int64_t)up != -(int64_t)down || up == 0) {
		printf("FAIL reversed bounds: %d and %d\n", (int)up,
		       (int)down);
		failures++;
	}

	/* An empty tree has no integral. */
	if (gexpr_integrate(&empty, 0, Q16_ONE, &zero) != GEXPR_INT_UNBOUNDED ||
	    gexpr_integrate(NULL, 0, Q16_ONE, &zero) != GEXPR_INT_UNBOUNDED) {
		printf("FAIL an empty tree was integrated\n");
		failures++;
	}

	/* A singularity inside the interval has no finite integral, whether it
	 * lands on a sample or only shows up as the refinement closes in. */
	expect_refused_integral("1/x", -Q16_ONE, Q16_ONE);
	expect_refused_integral("1/(x-1)", 0, 2 * Q16_ONE);
	expect_refused_integral("1/(x-1)", 13107, 98304);
	/* A domain where the function has no value. */
	expect_refused_integral("sqrt(x)", -Q16_ONE, Q16_ONE);
	/* The result, not the samples, leaves Q16. */
	expect_refused_integral("x^2", 0, 300 * Q16_ONE);
	expect_refused_integral("exp(x)", 0, 50 * Q16_ONE);
}

/* ---- the typed integral form ---- */

static void expect_integral_typed(const char *text, const char *want)
{
	char got[64];

	if (gexpr_integral_answer(text, got, sizeof got) < 0) {
		printf("FAIL typed %s -> refused, wanted %s\n", text, want);
		failures++;
		return;
	}
	if (strcmp(got, want)) {
		printf("FAIL typed %s -> %s, wanted %s\n", text, got, want);
		failures++;
	}
}

static void test_integral_typed(void)
{
	expect_integral_typed("integrate(x^2,0,3)", "9");
	expect_integral_typed("integrate(x^2, 0, 3)", "9");
	expect_integral_typed("int(x^2,0,3)", "9");
	expect_integral_typed("int (x^2 , 0 , 3)", "9");
	expect_integral_typed("integrate( sin(x) , 0 , 3.14159 )", "2");
	expect_integral_typed("int(sqrt(x),0,1)", "0.6667");
	expect_integral_typed("integrate(1/x,1,4)", "1.386");
	expect_integral_typed("integrate(abs(x),-2,3)", "6.5");
	expect_integral_typed("integrate(x^2,3,0)", "-9");
	expect_integral_typed("integrate(-x^2,0,3)", "-9");
	expect_integral_typed("integrate(x^2,0.5,1.5)", "1.083");
	expect_integral_typed("integrate(x^2,-1.5,-0.5)", "1.083");
	expect_integral_typed("integrate(5,0,2)", "10");
	expect_integral_typed("integrate((x+1)^2,0,2)", "8.667");

	/* A bound is a constant expression, evaluated once in Q16 before the
	 * integral runs. pi therefore means 3.14159, not pi: integrate(x,0,pi)
	 * is 4.93478... and prints 4.935. */
	expect_integral_typed("integrate(x,0,pi)", "4.935");
	expect_integral_typed("integrate(sin(x),0,pi)", "2");
	expect_integral_typed("integrate(x,0,2*pi)", "19.74");
	expect_integral_typed("integrate(x,0,2pi)", "19.74");
	expect_integral_typed("integrate(x,0,pi/2)", "1.234");
	expect_integral_typed("integrate(x^2,0,3/2)", "1.125");
	expect_integral_typed("integrate(1/x,1,exp(1))", "1");
	expect_integral_typed("integrate(cos(x),0,pi/2)", "1");
}

static void test_integral_typed_refusals(void)
{
	static const char *const refused[] = {
		/* the whole text has to be the call */
		"integrate(x^2,0,3)+1", "2*int(x^2,0,3)", "integrate (x^2,0,3)junk",
		/* not three parts */
		"integrate(x^2,0)", "integrate(x^2,0,3,4)", "int(x^2)", "",
		/* brackets that do not balance */
		"integrate(x^2,0,3", "integrate(x^2,0,3))", "integrate(x^2,0,3,",
		/* bounds that are not constant expressions, depend on x, have no
		 * finite Q16 value, or do not fit */
		"integrate(x^2,a,b)", "integrate(x^2,0,2^)",
		"integrate(x,0,x)", "integrate(x,0,x^2+1)", "integrate(x,0,1/0)",
		"integrate(x,0,exp(50))", "integrate(x^2,0,40000)",
		/* not the call */
		"integr(x^2,0,3)", "integratex(x^2,0,3)", "intx(x^2,0,3)",
		/* a real integral the engine cannot bound */
		"integrate(1/x,-1,1)", "integrate(1/(x-1),0,2)",
	};
	char got[64];

	for (size_t i = 0; i < sizeof refused / sizeof refused[0]; i++) {
		if (gexpr_integral_answer(refused[i], got, sizeof got) == 0) {
			printf("FAIL typed %s -> %s, wanted no answer\n",
			       refused[i], got);
			failures++;
		}
	}
	/* Two bounds that are the same Q16 value are a zero-width interval,
	 * which is an answer rather than a refusal. */
	expect_integral_typed("integrate(x^2,0.000001,0.000002)", "0");
}

/* ---- failures that must stay failures ---- */

static void expect_derive(const char *text, int want)
{
	gexpr_t f, d;
	char err[48];
	int got;

	if (gexpr_parse(text, &f, err, sizeof err) < 0) {
		printf("FAIL %s does not parse: %s\n", text, err);
		failures++;
		return;
	}
	got = gexpr_derive(&f, &d);
	if (got != want) {
		printf("FAIL %s: derive returned %d, wanted %d\n", text, got, want);
		failures++;
		return;
	}
	/* A refused derivative must not leave a readable tree behind, or a
	 * caller would print half of one. */
	if (got != GEXPR_DIFF_OK && d.root != -1) {
		printf("FAIL %s: refused but left root %d\n", text, d.root);
		failures++;
	}
}

static void test_failures(void)
{
	static const char *const refused[] = {
		"d/dx(2^x)", "d/dx(x^", "d/dx()", "d/dx", "diff",
		"d/dx(x^2)+1", "d/dx(2x) d/dx(x)", "1/dx(x^2)",
		"d/dx(sin(x)+)", "differentiation of x",
	};
	gexpr_t empty = { .n = 0, .root = -1 }, d;
	char out[128];
	char tiny[2];

	/* An exponent that depends on x needs a rule this engine does not have. */
	expect_derive("2^x", GEXPR_DIFF_UNSUPPORTED);
	expect_derive("x^x", GEXPR_DIFF_UNSUPPORTED);
	expect_derive("sin(x)^x", GEXPR_DIFF_UNSUPPORTED);

	/* A product of enough factors does not fit beside its own source. */
	expect_derive("sin(x)*cos(x)*tan(x)*exp(x)*ln(x+2)*sqrt(x+2)*(x^2+1)"
	              "/(x+3)", GEXPR_DIFF_TOO_BIG);

	if (gexpr_derive(&empty, &d) == GEXPR_DIFF_OK ||
	    gexpr_unparse(NULL, out, sizeof out) == 0 ||
	    gexpr_unparse(&empty, out, sizeof out) == 0) {
		printf("FAIL an empty tree was accepted\n");
		failures++;
	}

	/* The typed path refuses rather than guesses. */
	for (size_t i = 0; i < sizeof refused / sizeof refused[0]; i++) {
		if (gexpr_derivative_answer(refused[i], out, sizeof out) == 0) {
			printf("FAIL %s -> %s, wanted no answer\n", refused[i],
			       out);
			failures++;
		}
	}

	/* A buffer one byte short fails instead of writing half a tree. */
	{
		gexpr_t f;
		char err[48];

		if (gexpr_parse("x^2+1", &f, err, sizeof err) < 0 ||
		    gexpr_derive(&f, &d) != GEXPR_DIFF_OK) {
			printf("FAIL setup for the short buffer\n");
			failures++;
			return;
		}
		if (gexpr_unparse(&d, tiny, sizeof tiny) == 0) {
			printf("FAIL unparse into a 2 byte buffer did not fail\n");
			failures++;
		}
	}
}

/* ---- the typed-answer form ---- */

static void expect_typed(const char *text, const char *want)
{
	char got[64];

	if (gexpr_derivative_answer(text, got, sizeof got) < 0) {
		printf("FAIL typed %s -> no answer, wanted %s\n", text, want);
		failures++;
		return;
	}
	if (strcmp(got, want)) {
		printf("FAIL typed %s -> %s, wanted %s\n", text, got, want);
		failures++;
	}
}

static void test_typed(void)
{
	expect_typed("d/dx(x^3+2x)", "3x^2+2");
	expect_typed("d/dx (x^3+2x)", "3x^2+2");
	expect_typed("diff(x^2)", "2x");
	expect_typed("derivative(sin(x))", "cos(x)");
	expect_typed("d/dx((x+1)^2)", "2(x+1)");
	expect_typed("d/dx(5)", "0");
	expect_typed("d/dx(x)", "1");
}

int main(void)
{
	test_cases();
	test_abs_sides();
	test_in_place();
	test_generated();
	test_integrals();
	test_integral_edges();
	test_integral_typed();
	test_integral_typed_refusals();
	test_failures();
	test_typed();

	printf("worst finite-difference disagreement: %d permille (%s)\n", worst_fd,
	       worst_fd ? worst_fd_where : "none");
	printf("worst print and re-parse disagreement: %d permille (%s)\n",
	       worst_print, worst_print ? worst_print_where : "none");
	printf("printed forms past the %d node parse limit: %d (round trip not "
	       "checked)\n", EXPR_MAX_NODES, too_big_to_print);
	printf("worst definite-integral disagreement: %d ppm (%s)\n",
	       worst_int, worst_int ? worst_int_where : "none");

	if (failures) {
		printf("%d failure(s)\n", failures);
		return 1;
	}
	printf("expression differentiation and integration: all checks passed\n");
	return 0;
}
