# Wave 4: the plan

Planning document. Nothing here is implemented yet; `docs/waves.md` holds the
workstream list this expands, and its status table is the truth about what is on
master.

Wave 4 is the last wave in the plan, and it is not one kind of work. It contains
three of the hardest things in the project (4.1–4.3), the last big block of
formula breadth (4.4), the product work that makes any of it usable (4.5), and
graphing that only this architecture can do (4.6). They have different risks,
different verification, and only two of the six need a retrain.

## What orders the work

**The capacity decision gates 4.4, and nothing else.** Measured on the same
1,840 hand-written rows, accuracy is 72.6% at 84 relations, 73.8% at 90 and
72.3% at 102 — flat — while the generated gate fell 95.0%, 94.2%, 90.5%.
Training longer does not recover it: the last run reached validation loss 0.0001
by step 12,000. So the model is out of capacity, not out of data or steps.

That matters here because 4.4 is roughly 34 more relations. Adding them costs a
dataset rebuild and an 80-minute retrain, and the evidence says a 136-relation
catalogue on a 256-dimension model will be worse at the relations that already
work. Either the model gets wider first (about twice the answer time on a 132 MHz
ARM9, unmeasured on hardware), or 4.4 ships with the accuracy cost recorded, or
4.4 waits.

Everything else in wave 4 is unaffected: 4.1–4.3 and 4.6 are typed-expression
code that never touches the model, and 4.5 is device UI. They can run in
parallel with the capacity decision rather than blocked on it.

## Dependency map

| Workstream | Depends on | Blocks |
| --- | --- | --- |
| 4.1 symbolic integration | — | 4.3 (separable and first-order linear ODEs), 4.2's error estimates |
| 4.2 series | 4.1 partially: no, but a simplifier from 4.1 is reused | — |
| 4.3 differential equations | 4.1 for closed forms; RK4 alone depends on nothing | 4.6f (slope fields need somewhere to be plotted) |
| 4.4 waves/optics/fluids/modern | capacity decision; new solver unit | — |
| 4.5 interface | — | 4.6a–4.6e (they need a menu to hang off) |
| 4.6 graphing | 4.5 for entry, `pipeline` top-3 margins for 4.6c | — |

## 4.1 Symbolic integration

**What ships:** `integrate(3x^2 + 2x)` answers `x^3 + x^2 + C`. Today
`integrate()` only takes bounds and returns a number; there is no antiderivative
as an expression anywhere in the codebase.

**Why it is the biggest item.** Four things have to exist before one rule can be
written:

| # | Task | File | Size | Done when |
| --- | --- | --- | --- | --- |
| a | Grow the expression pool from 48 nodes; measure stack on device | `runtime/expr.h` | small | a 3-plugin tree parses and evaluates without saturating, and the on-device build still fits |
| b | Simplifier: constant fold, `x*1`, `0*f`, `f^1`, combine like terms | new `runtime/simplify.c` | medium | by-parts output of `x^2*e^x` prints at under ~40 nodes instead of exploding past the pool |
| c | Rule table: power, `1/x`, `e^x`, `sin`, `cos`, `ln`, linearity, `f(ax+b)` substitution | `runtime/expr.c` | large | each rule round-trips: differentiate the answer and get the integrand back |
| d | Unparse the new forms (rational answers, `ln`, multiple terms) | `runtime/expr.c` | small | `integrate(1/x)` prints `ln|x|` and reparses to the same tree |
| e | Wire the typed path: `integrate(f)` without bounds; keep the bounded form | `runtime/expr.c`, `pipeline.c` | small | `integrate(x^2)` returns `x^3/3` and `integrate(x^2,0,3)` still returns 9 |

**The gate is a round trip, not a table.** For every generated integrand,
differentiate the antiderivative numerically and compare against the integrand
at random points in the domain. That checks every rule without hand-written
expected values, and it catches the failure this workstream will actually
produce — a plausible-looking antiderivative that is silently off by a factor.

**Risk:** tree growth. Differentiation multiplies nodes by roughly two; by-parts
and partial fractions multiply by more, and the pool is a fixed array because
there is no heap worth using on this target. The simplifier is not polish, it is
what keeps the feature inside the pool, so it ships before the rule table rather
than after.

## 4.2 Series

**What ships:** `series(sin(x), 0, 5)` answers the Maclaurin polynomial;
convergence answers are limited to what can be decided by pattern.

| # | Task | Size | Done when |
| --- | --- | --- | --- |
| a | Taylor and Maclaurin coefficients by repeated differentiation at a point, with factorial scaling | medium | `series(e^x,0,4)` gives `1 + x + x^2/2 + x^3/6 + x^4/24` |
| b | Unparse the polynomial with descending powers, or ascending — pick one and say so | small | output reparses to the same tree |
| c | Error estimate: evaluate the series and the function at points inside the radius; report the largest gap | small | reported error matches the measured gap |
| d | Convergence: ratio test and geometric recognition | medium | geometric series decide correctly; everything else reports "cannot decide" rather than guessing |

**Honest limit:** a real convergence test needs limits, and this engine has no
symbolic limit. Pattern recognition covers geometric, p-series and alternating
series and stops there. The answer to "does this converge" beyond those cases is
a refusal, which is better than a coin flip.

## 4.3 Differential equations

**What ships:** `ode(y' = 2y, 0, 1, 10)` traces numerically; separable and
first-order linear equations answer in closed form once 4.1 exists.

| # | Task | Size | Done when |
| --- | --- | --- | --- |
| a | Two-variable evaluation: `y' = f(x,y)` needs an evaluator that is not `eval(x)` | medium | the node evaluator takes an environment instead of a single `x` |
| b | RK4 with a fixed step and a table of values | medium | exponential growth from `y'=2y` matches the closed form to four significant digits over ten steps |
| c | Separable and first-order linear closed forms, built on 4.1 | large | cooling, RC and logistic examples match their closed forms |
| d | Slope-field data for 4.6f | small | the field renders from the same evaluator |

**Risk:** the two-variable change touches `gexpr_eval`, which every typed answer
and the whole graph mode use. It is the one change in wave 4 that can regress
something already shipped, so it goes behind the existing `test_typed` and
`test_graph` gates before anything else is built on it.

## 4.4 Waves, optics, fluids, modern physics

**What ships:** the rest of AP Physics 2, about 34 relations:

| Topic | Relations | Examples |
| --- | --- | --- |
| Waves and sound | 7 | interference path difference, standing-wave frequency, sound intensity level, Doppler shift, beat frequency |
| Optics | 6 | Snell's law, critical angle, lens and mirror equation, magnification |
| Fluids | 7 | gauge pressure, Pascal, buoyancy, continuity, Bernoulli, flow rate |
| Modern physics | 9 | photon energy, photoelectric work function and stopping potential, de Broglie wavelength, Bohr energy levels, radioactive activity, mass-energy |
| Remaining thermo/gas | 5 | RMS speed, mean free path, adiabatic work, Carnot efficiency as a function |

Cost per relation is measured, not estimated: `relations/physics.py` is 14 lines
per relation and `runtime/solvers_physics.c` is 27, plus a surface template and
a parity vector. The expensive part is not the writing, it is the retrain and
the full gate that every catalogue change forces.

**Gated on the capacity decision.** If it ships as-is, the plan should predict
in advance that the generated gate falls again and record it, the way wave 3 did,
rather than discovering it after the retrain.

## 4.5 Interface

**What ships:** the app stops being two `show_msgbox` calls. Today `main.c` is
151 lines and the whole UI is a text prompt and an answer box.

| # | Task | Size | Done when |
| --- | --- | --- | --- |
| a | A real screen loop: menu, input line with editing, scrollable answer area | large | every existing path (typed, word problem, graph) is reachable without a message box |
| b | Error explanations mapped from the pipeline's failure codes | medium | a refused call says which of the three failures it was: no model loaded, argument not in the question, malformed call |
| c | Step display | large, and a decision — see below | the answer screen shows the formula with the substituted values before the result |
| d | History: last N questions and answers in RAM, recall with one key | medium | answers survive leaving and re-entering the screen |
| e | Keypad templates: on-screen help for the typed syntax | small | `integrate(x^2,0,3)` and `d/dx` are discoverable without the manual |

**Step display is a fork in the road.** The generic version prints the relation
name, its arguments in order, and the result — cheap, works for all 102
relations, and is nearly useless to a student. The useful version prints the
actual formula with numbers substituted, which needs every relation to describe
its own formula: about five lines each, so roughly 500 lines of data and a new
required field in the registry. It is the single highest-value item in 4.5 and
the most expensive; it should be decided deliberately rather than drifting into
the cheap version.

## 4.6 Graphing

Six items, planned in `docs/waves.md`. Sizing here:

| # | Item | Size | Note |
| --- | --- | --- | --- |
| a | Words to curve | medium | feed the chosen call into `graph_init` instead of a typed expression |
| b | The answer drawn where it belongs | small | one overlay in `graph_render`; the cheapest trust win in the wave |
| c | Why this formula: top-3 candidates dimmed | medium | `pipeline` uses a bare `argmax32`; it has to keep the top three call tokens and their margins |
| d | Parameter families | small | loop one argument and plot each |
| e | Diagram answers | large | per-subject drawing: vectors, strobes, shaded regions |
| f | Slope fields and Euler traces | medium | renders from 4.3a's evaluator |

## Recommended sequence

**Phase 0 — make what exists trustworthy, no retrain and no new engine.**
4.5b (error explanations), 4.6b (the answer drawn on its curve), 4.5d (history).
Small, independent, and they attack the trust problem the project already named:
a correct answer nobody believes is not a product.

**Phase 1 — the expression engine.** 4.1, then 4.2, then 4.3. Strictly
sequential, because 4.3's closed forms need 4.1, and 4.1's simplifier is what
makes the rest fit in fixed memory. None of it needs a retrain, so none of it is
blocked on the capacity decision.

**Phase 2 — the product.** 4.5a, then step display (4.5c) once its shape is
decided, then 4.6a and 4.6c.

**Phase 3 — breadth, after the capacity decision.** 4.4, in two batches so the
second can be abandoned without losing the first.

**Phase 4 — the expensive flourishes.** 4.6e diagram answers, 4.6f slope fields.

## Decisions this plan needs

1. **Capacity: wider model, runtime argument verification, or neither yet.**
   Gates 4.4 and sets what accuracy wave 4 can claim.
2. **Step display: generic or per-relation.** A cheap line that does not help, or
   roughly 500 lines of registry data that does.
3. **Vocabulary headroom.** The 512-token vocabulary was sized for about 250
   relations, so 136 names fit. No action needed unless 4.4 grows past that.

## Not promised

No computer algebra system. 4.1 is an integration rule table with a simplifier,
not SymPy: it will not do arbitrary algebraic manipulation, and it will refuse
rather than guess. No symbolic limits, so convergence answers stay pattern-based.
No 3D graphing, polar or parametric entry, and no animated sliders — those need a
keyboard workflow this device does not have.
