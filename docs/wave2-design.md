# Wave 2 design sketches

Written before implementation so the work can be handed out without re-deriving
the shape. Both sketches assume wave 1 is merged: the expression engine in
`runtime/expr.c` and the per-subject formula tables.

## 2.1 Solve one equation for any unknown

**The temptation, and why to resist half of it.** The catalogue promises 2.5-4x
the questions per relation slot by replacing hand-written inverses with one
equation plus a generic solve. That promise has two halves:

- the **implementation** win: one equation spec instead of one C function per
  direction, so `ohm_v`, `ohm_i` and `ohm_power` stop being three hand-written
  solvers;
- the **vocabulary** win: one token per equation instead of one per direction.

The implementation win is cheap and safe. The vocabulary win changes the call
grammar the model was trained on, which is the single riskiest change available
to this project, and it buys something the corpus does not yet need: 66
relations is nowhere near the 1,024-token ceiling wave 1 raised.

**Decision: take the implementation win now, defer the contract change until
there is a sprawl of inverses to collapse.**

Sketch, phase A (no model change, no retrain):

```c
/* runtime/equation.h */
typedef struct {
	const char *name;           /* "ohm" */
	int nq;                     /* quantities V, I, R */
	const char *qname[6];       /* display names, in canonical order */
	/* Given every quantity but slot, write the value of slot as a rational. */
	int (*solve)(int slot, const struct ratio *given, struct ratio *out);
} equation_t;
```

A relation becomes a thin adapter: `ohm_i(V, R)` calls `equation_solve("ohm", Q_I,
given)`. `test_solvers` must stay green with identical strings, which is the
proof the adapter is faithful.

Phase B (contract change, only when the inverse sprawl justifies it): the call
becomes `CALL eq.<name> <slot> <known...>`, arity `1 + (nq - 1)`. The unknown is
absent from the question by construction, so the grounding guard needs a rule
for "the unknown is the one field that must not appear". Do not start phase B
before phase A has shipped and the accuracy of the extra slot-index token has
been measured on one subject.

## 2.2 Symbolic differentiation

The parser and evaluator already exist in `runtime/expr.c`. Differentiation is a
tree rewrite plus a simplifier, and the simplifier matters more than the rules:
without it the tree doubles in size at every application and hits the node cap
on a second derivative.

Sketch:

```c
/* runtime/expr.h */
#define EXPR_MAX_DIFF 96        /* a differentiated tree is bigger than its source */

int gexpr_derive(const gexpr_t *src, gexpr_t *out);   /* out = d/dx src */

/* One call does what a student types: "d/dx(3x^2 sin x)" reports the derivative
 * as an expression, evaluated at x when the text also gives one. */
int expr_derivative_text(const char *text, gexpr_t *out, char *err, int errlen);
```

Rules, in the order they are easiest to test: constant, `x`, sum, product,
quotient, power with a constant exponent, then `sin`, `cos`, `tan`, `ln`, `exp`,
`sqrt`, and the chain rule over any of them. Integer constants stay exact;
`gq_mul`/`gq_div` already saturate, so no new arithmetic is needed.

Simplifier, deliberately minimal: `0 + a`, `a + 0`, `a - 0`, `1 * a`, `a * 1`,
`0 * a`, `a ^ 1`, `a ^ 0`, and folding two numeric children. Anything more is a
CAS, and the plan says not to build one.

**The check that actually matters** is not a table of expected strings — it is
agreement with a numerical derivative. For randomly generated expressions over
the supported operators, compare `gexpr_eval(derived, x)` against a central
difference of `gexpr_eval(source, x)` at twenty sample points, and require
agreement to within the Q16 step scaled by the magnitude. Hand-written cases
cover the textbook examples; the property test covers the ones nobody thought
of.

## What wave 2 hands to agents

| Workstream | Owns | Depends on |
| --- | --- | --- |
| 2.1 equation engine, phase A | new `runtime/equation.c/h`, `runtime/solvers_math.c` | merged wave 1 |
| 2.2 differentiation | `runtime/expr.c`, new `runtime/test_expr.c` | merged wave 1 |
| 2.3 curve analysis | `runtime/graph.c` | 2.2 for second derivatives |
| 2.4 AP Physics 1 mechanics | `relations/physics.py`, `runtime/solvers_physics.c` | merged wave 1 split |

2.3 and 2.4 can start with the rest; 2.3 needs 2.2 only for concavity, which can
be a numeric second difference until the symbolic version lands.
