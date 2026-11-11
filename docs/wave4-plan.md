# Wave 4: the plan

`docs/waves.md` holds the workstream list this expands, and its status table is
the truth about what is on master. This page is the shape of the work and where
each piece stopped.

## Where wave 4 stands

| # | Workstream | State | What is left |
| --- | --- | --- | --- |
| 4.1 | Symbolic integration | **done** | powers of x, sums, constant multiples, `1/x` to `ln(x)`, and sin/cos/exp of a linear argument; anything else is refused |
| 4.2 | Series | **half** | Taylor and Maclaurin answer as expressions. Convergence tests are not built: a real ratio test needs limits and this engine has none |
| 4.3 | Differential equations | **done** | RK4 answers a value at a point; `desolve` answers the closed form for growth, decay, cooling and logistic equations, including a rate that depends on x (`desolve(x*y,0,2)` is `2exp(x^2/2)`, `desolve(x/y,0,1)` is `sqrt(1+x^2)`). First-order linear with a non-zero x-dependent forcing term is still refused, and slope-field data exists for 4.6f with nothing yet drawing it |
| 4.4 | Waves, optics, fluids, modern physics | **done** | 34 relations, 136 in the catalogue, and the predicted capacity cost did not appear: at the recorded training protocol both accuracy gates sit within a point of wave 3's. Three relations were cut for the numeric range, not for time |
| 4.5 | Interface | **done** | a screen model in `runtime/ui.c` — menu, editing input line, paged answer pane with a scroll bar, eight-pair history, three pages of keypad help — with `device/main.c` down to 61 lines of key mapping and blit. `test_screen` and `test_ui` are gate rows, and `crosscheck.sh` compiles and links the device path for `arm926ej-s`. Not verified: nothing has run on a calculator |
| 4.6 | Graphing from the problem | **done** | 4.6a `graph_from_call` plots the relation a call named and 4.6b marks the point the answer is, both reachable from the answer screen with VAR; 4.6c `pipeline_alternatives` keeps the top three candidates and their margins; 4.6d `graph_family_add` sweeps one argument; 4.6e `diagram.c` draws force arrows, motion strobes and shaded integrals on TAB; 4.6f `graph_field` draws a slope field. Each has a host test; none has been looked at on hardware |

Everything in that table is host-verifiable and gated, and the last two rows are
no longer the exception they were planned as. The plan said 4.5 and 4.6 could be
written here but not compiled, because no machine had the Ndless toolchain. That
was wrong, and `docs/wave4-ui-plan.md` records why: the device sources compile
for `arm926ej-s` against the vendored SDK headers alone, and `crosscheck.sh` now
compiles and links them on every gate run, so a missing module breaks the gate
instead of the calculator.

The rule that made both workstreams buildable is the one `graph.c` already
followed: all the logic lives in `runtime/`, where a host test can drive it with
synthetic keys and assert on the framebuffer, and the Ndless file is reduced to a
binding with no decisions in it. What is still out of reach is running the
result. A `.tns` needs Ndless's own libraries and a calculator, so no screen in
this wave has been seen by a person and no timing has been measured.

The gate each engine piece carries is worth more than the feature: integration
differentiates its own answer back, series compares against the function it
approximates, and the ODE closed form is checked against the numerical solution
it was derived independently of. Each one caught wrong work of mine on the run
that introduced it.

Wave 4 is the last wave in the plan, and it is not one kind of work. It contains
three of the hardest things in the project (4.1–4.3), the last big block of
formula breadth (4.4), the product work that makes any of it usable (4.5), and
graphing that only this architecture can do (4.6). They have different risks,
different verification, and only two of the six need a retrain.

## What orders the work

**The capacity decision gated 4.4, and 4.4 answered it.** Measured on the same
hand-written rows, accuracy was 72.6% at 84 relations, 73.8% at 90 and 72.3% at
102 — flat — while the generated gate fell 95.0%, 94.2%, 90.5%. That read as a
model out of capacity, and it was the reason the last 34 relations waited behind
a decision about making the model wider.

The prediction did not survive its own test. At 136 relations, trained the way
the recorded numbers were trained, the generated gate reads **90.3%** and the
hand-written paraphrases **71.8%** — both within half a point of the 102-relation
figures, with 1,942 hand-written rows covering the new relations rather than
generated phrasings alone. The wider model is not the binding constraint at this
size, and the answer time it would have cost stays unspent.

What did move was the recipe, and it moved by more than the catalogue did. The
Makefile's defaults — 6,000 steps and no paraphrase mix — score 83.6% and 32.7%
on this same catalogue. That is a 7-point and a 39-point gap that has nothing to
do with how many relations exist. `make data` now mixes the corpus's train split
and `make train` runs the 12,000 steps the published numbers came from, because
an accuracy figure whose recipe is not the default recipe is a figure nobody can
reproduce.

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
   Answered by 4.4 and not in the direction the plan expected: 34 more relations
   cost half a point on both gates, so no wider model is needed at this size.
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
