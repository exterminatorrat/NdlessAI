# Execution plan: waves and workstreams

Locked 2026-11-08. This is the file that answers "what are we doing next, who
owns it, and what does done mean". Update the status table when a workstream
changes state; never leave a row claiming something that is not true.

The capability catalogue lives in [`capabilities.md`](capabilities.md); the
value ranking behind the wave order lives in [`expansion.md`](expansion.md).

## How work is run

1. **Plan first.** Wave scope is agreed before code. Within a wave, workstreams
   are independent slices with one owner each.
2. **One PR per concern, under 1,000 changed lines.** Anything larger is split
   or stacked. A PR that needs three rounds of "fix, rerun, fix" is a sign it
   was too big, not that the reviewer was picky.
3. **Implement before building.** Add the capability, verify it with the
   nearest check that covers it, commit, open the PR. Full builds, toolchain
   work, packaging and unrelated cleanup are separate tasks unless the change
   cannot be verified without them.
4. **Parallel agents own distinct files.** Two agents never edit the same file
   in the same wave; the ownership matrix below is what makes that possible.
   Dependencies stack: the blocked PR branches on the one it needs.
5. **Verify the artifact, not the diff.** Every PR body carries the command it
   was checked with and the output. A check that could not run is reported as
   not run, with the reason.
6. **Report in outcomes.** Each wave closes with a non-technical summary: what
   a student can now do that they could not before, and what got faster or more
   reliable. Numbers, not adjectives.

## The honest budget

The full catalogue is roughly 250 new relations at about 100 lines each, plus
four engine modules and the interface work: on the order of 25,000 changed
lines, which at the 1,000-line PR limit is a floor of twenty-five PRs before
any review or verification time. Parallel agents compress the writing, not the
verification, and every relation batch needs a retrain before the model can
actually recognise it.

So: wave 0 and wave 1 in about a day of wall clock with four to six parallel
workstreams. Wave 2 in two to three days. Waves 3 and 4 are weeks of formula
work and measurement, whoever writes it. Anyone promising all four inside 24
hours is promising something that would not be verified.

## Wave 0 — clear the road (must finish before parallel work)

Parallelism is currently impossible: `solvers.c` is one 2,865-line file,
`relations.py` is 1,209 lines and `surface.py` is 1,008, so every relation
batch touches the same three files. The artifacts that the accuracy and parity
gates need are also missing from a fresh checkout.

| # | Workstream | Why it is first | Owns | Done when |
| --- | --- | --- | --- | --- |
| 0.1 | **Split the monoliths** — `solvers.c` into per-subject units behind one dispatch table; `relations.py` and `surface.py` into per-subject modules | Without this, four agents collide in one file and every merge costs more than the work | `runtime/solvers*.c`, `relations/*.py` | `test_solvers` still 13,335 assertions, 0 mismatches; every call in `test_solvers.c` reaches the same solver |
| 0.2 | **Build the artifacts** — `make data`, `make train`, `make export` | Parity, accuracy and every retrain need `weights.bin` and `data/`; today they are gitignored and absent | nothing committed | `weights.bin` exists; the fixed-point parity gate runs instead of skipping |
| 0.3 | **Record the baseline** — `evaluate_device` over 1,000 held-out rows | Every later wave must not regress this, and a number nobody wrote down cannot regress visibly | this file | exact-call and wrong-relation percentages recorded below |

## Wave 1 — speed and correctness (no new model capability)

A nine-second answer reads as a broken calculator, so this wave buys time and
trust before any breadth.

| # | Workstream | What it does | Owns | PR size |
| --- | --- | --- | --- | --- |
| 1.1 | **Invalid output blocking** — trie mask over relation name, arity and numeric slots | Makes a malformed call structurally impossible instead of merely rejected | new `runtime/callmask.c`, `pipeline.c` | ~400 |
| 1.2 | **Faster multiply-add loop** — unroll, four accumulators, schedule around the load latency in the int8 dot product | The kernel runs at 182 ms per token against a 74 ms bandwidth floor; the headroom is real | `runtime/fixed.c`, `test_fixed.c` | ~300 |
| 1.3 | **Wider vocabulary** — 1,024 tokens with a bigger merge budget | Shorter prompts, which is directly less prefill, and room for 300 relations instead of 250 | `tokenizer.py`, `train.py`, `export_tokenizer.py` (+ retrain) | ~300 |
| 1.4 | **Model optional at startup** — typed maths needs no weights | A calculator that answers arithmetic should not require a 5.9 MB file first | `device/main.c` | ~150 |

## Wave 2 — the multipliers

| # | Workstream | What it does | Owns | PR size |
| --- | --- | --- | --- | --- |
| 2.1 | **Solve for any variable** — one equation, any unknown | Replaces every hand-written inverse formula: roughly 2.5-4x the questions per relation slot | `relations/precalc.py`, `runtime/solvers_math.c` | ~900 |
| 2.2 | **Differentiation** — product, quotient, chain, higher order, implicit | All of AP Calc AB differentiation, on expressions nobody anticipated | `runtime/expr.c`, `test_expr.c` | ~700 |
| 2.3 | **Curve analysis** — roots, extrema, inflection, monotonicity, asymptotes | "Find the critical points of…" answered from the expression | `runtime/graph.c` | ~500 |
| 2.4 | **AP Physics 1 mechanics** — friction, inclines, Atwood, circular force, energy, collisions, torque, rotation, springs, waves, lenses | The largest unserved subject | `relations/physics.py`, `runtime/solvers_physics.c` (+ retrain) | ~900 |

## Wave 3 — breadth

| # | Workstream | What it does | Owns |
| --- | --- | --- | --- |
| 3.1 | **Numeric integration** — definite, area, volume of revolution, arc length, average value | Calc AB applications | `runtime/expr.c` |
| 3.2 | **Exact linear algebra** — n x n solve, determinant, inverse, rank | Systems, Kirchhoff, chemistry balancing | `runtime/solvers_math.c` |
| 3.3 | **AP Chemistry** — stoichiometry, solutions, equilibrium, kinetics, thermochemistry | Unit 4 first, then 7-8 | `relations/chemistry.py`, `runtime/solvers_chemistry.c` |
| 3.4 | **Thermodynamics** — calorimetry, latent heat, gas processes, Carnot, entropy, conduction, radiation | Mostly formulas, high yield per line | `relations/thermo.py`, `runtime/solvers_thermo.c` |
| 3.5 | **Electricity and magnetism** — Coulomb, Gauss, potential, capacitors, RC, Kirchhoff, induction | AP Physics C E&M | `relations/em.py`, `runtime/solvers_em.c` |
| 3.6 | **Real wording data** — hand-written and paraphrased questions in training, adversarial distractors, and `stress_cases.py` promoted to a gate | The measured 91.6% is on generated phrasings; this is the accuracy lever | `data/`, `evaluate_*.py` |

## Wording, measured — the number that changes the plan

Every accuracy figure recorded above was measured on generated questions: the
same templates the training data is built from, with different numbers. On
hand-written questions the same model scores far worse, and that is the number a
student experiences.

| Measurement, 588 hand-written rows | Generated phrasings | Hand-written, before | Hand-written, after the wording retrain |
| --- | --- | --- | --- |
| Exact call | 92.9% | 43.4% | **61.2%** |
| Right relation, wrong arguments | 4.7% | 22.8% | 19.7% |
| Wrong relation | 2.5% | 33.8% | **19.0%** |
| Generated-set exact call, after | — | 92.9% | 89.5% |

The wording retrain put 1,260 hand-written questions over all 84 relations into
the corpus, split six train and four eval per relation by a hash of the
question, and made a fifth of the training set those training rows. The eval
rows are unreadable by `make_dataset`, so the before-and-after is on wording no
version of the model has seen.

The cost is real and belongs in the same table: generated-phrasing accuracy fell
3.4 points and the wrong-relation rate on generated rows rose 2.1% to 3.9%. That
is capacity moved off template recall and onto natural wording. A student types
natural wording, so the trade is right, but a plan that quotes only 61% would be
hiding the price.

**The lesson, in one line:** a generated held-out set measures phrasing recall,
not understanding, and any accuracy claim in this project has to name which set
it came from.

### Where the wording curve flattened, and what that decides

Four rounds of hand-written data, each measured on the rows the model never
trained on. The first three paid; the fourth did not.

| Round | Hand-written rows | Exact, hand-written | Wrong formula | Exact, generated gate |
| --- | --- | --- | --- | --- |
| none | 0 | 43.4% (588) | 33.8% | 92.9% |
| one | 1,260 | 61.2% (588) | 19.0% | 89.5% |
| two | 2,710 | 67.6% (1,168) | 12.9% | 91.6% |
| three | 4,390 | 72.6% (1,840) | 10.1% | 95.0% |
| four | 6,070 | 68.0% (2,512) | 9.8% | 92.1% |

Round four was aimed at the failure that dominates by then — right formula,
wrong arguments (17.3%) — by burying the real data among distractors. Compared
like-for-like against round three **on the same 2,512 rows**, it moved exact from
66.7% to 68.0%, a 1.3-point gain with a standard error of about 0.9 points, so
indistinguishable from zero. The generated gate moved 95.0% to 92.1%, which is
3.4 standard errors, so that one is real.

It is not shipped. The corpus stays on a branch with these numbers attached, and
the shipped model is round three: 72.6% exact and 10.1% wrong on hand-written
questions, 95.0% and 0.6% on the generated gate.

**The decision this forces:** about 6,000 hand-written questions is where data
stops buying accuracy on a 256-dimension model, and the next lever is not a
fifth round. It is either capacity — roughly twice the answer time on a
calculator that already takes seconds — or argument-level verification in the
runtime, which is a code change and not a data one. Both need choosing
deliberately rather than by another authoring round.

## Wave 4 — the deep end

| # | Workstream | What it does | Owns |
| --- | --- | --- | --- |
| 4.1 | **Symbolic integration** — table, linearity, substitution, by parts, partial fractions | Completes Calc BC | `runtime/expr.c` |
| 4.2 | **Series** — Taylor, Maclaurin, convergence and error tests | The remaining BC unit | `runtime/expr.c` |
| 4.3 | **Differential equations** — separable, first-order linear, logistic, RK4, slope fields | Cooling, kinetics, RC, populations | `runtime/expr.c` |
| 4.4 | **Waves, optics, fluids, modern physics** | The rest of AP Physics 2 | `relations/waves.py`, `relations/optics.py`, `relations/fluids.py`, `relations/modern.py`, `runtime/solvers_{waves,optics,fluids,modern}.c` |
| 4.5 | **Interface** — keypad templates, step display, error explanations, history | A correct answer nobody believes is not a product | `device/*.c` |
| 4.6 | **Graphing from the problem, not from a formula** — see below | The one graph feature a competitor cannot copy from its own UI | `device/graph_ui.c`, `runtime/graph.c` |

### 4.6 — graphing that only this architecture allows

TI-Nspire's Graphs application is already good at plotting an expression the
student types: window settings, trace, zeros, extrema, intersections, `dy/dx`,
shaded integrals, area between curves, and on the newer OS, slope fields and
sliders. Rebuilding that is not worth the pixels, and on a 320x240 screen at
132 MHz it would be a worse copy.

The asymmetry worth exploiting is that this system knows **which relation the
question used and with what arguments**. So the graph can be of the problem
rather than of an expression the student had to derive first.

| # | Idea | Why it beats the TI graph app | Cost |
| --- | --- | --- | --- |
| a | **Words to curve**: the model picks the relation, its arguments fill in, and the result is plotted with no formula typed | TI needs the expression, and a student who can produce it usually did not need the calculator | medium — feed the chosen call into `graph.c` |
| b | **The answer drawn where it belongs**: the solved value appears as a labelled point on the curve it came from | The number and the picture agree visibly, which is the trust problem 4.5 exists to fix | small — one overlay path |
| c | **Why this formula**: plot the top three candidate relations the model weighed, dimmed, with the chosen one solid | TI has no concept of a candidate, so a near miss is silent; here it is visible | medium — needs the call-token margins |
| d | **Parameter families**: the same relation at 15, 30, 45 and 60 degrees on one screen, or three values of a friction coefficient | TI's sliders animate a function you typed; this sweeps the catalogue's own arguments | small — loop over one argument |
| e | **Diagram answers**: force vectors from `friction_force`, a motion strobe from `kinematic_v`, a shaded region from `integr_poly3` | The answer stops being a number on a text screen | large — per-subject drawing |
| f | **Slope fields and Euler traces**, so 4.3 has somewhere to show itself | Par with TI rather than ahead; listed because ODE work needs a display | medium |

What this deliberately is not: a CAS plotter. It stays behind TI on 3D
graphing, polar and parametric entry, and animated sliders, all of which assume
a keyboard workflow this device does not have. The claim worth making is
narrower and true — it graphs the problem, not just a function, and it shows
why one formula was chosen over the others.

## Status

Every row was checked against master at `2beba30`, not against the state of a
pull request. A closed pull request is not a missing workstream: most of the
wave-1 and wave-2 pulls were closed once their content reached an integration
branch, and that branch is what merged. The `Where it lives` column names the
file or gate that proves the row, so the claim can be rechecked without trusting
this table.

Waves 1 through 3 are on master through #51, #53 and #58–#62. The catalogue is
102 relations in ten subject modules.

| Workstream | State on master | Where it lives |
| --- | --- | --- |
| Typed expressions with no model (ahead of wave 1, closed 1.4's gap) | merged, #15 | `runtime/expr.c`; `typed_mode()` answers without `weights.bin` |
| 0.1 split the solver and formula tables by subject | merged, #19 | ten `relations/*.py` modules, seven `runtime/solvers_*.c` units |
| 0.2 build the artifacts | on the build machine only, not committed | `weights.bin` and `data/` are gitignored, so `gate.sh` skips parity and accuracy on a fresh clone |
| 0.3 record the baseline | done | the numbers under "Baseline measured on the build machine" |
| 1.1 invalid output blocking | merged, #20; linked into the device app in #58 | `runtime/callmask.c`, `callmask.o` in `device/Makefile` |
| 1.2 faster multiply-accumulate | merged, #21 | `runtime/fixed.c` |
| 1.3 vocabulary 1,024 | **closed as a measured regression**: 87.9% exact against 92.5% at 512 on the same 2,000 rows. The capacity bug it exposed is fixed in #23 | 512-token vocabulary |
| 1.4 typed maths without the weights file | merged with #15; #17 closed as folded | `typed_mode()` in `device/main.c` |
| 2.1 solve for any variable (equation engine) | merged; #25 closed as folded into the wave-2 integration | `runtime/equation.c` |
| 2.2 differentiation | merged; #27 closed as folded | `d_node()` in `runtime/expr.c` |
| 2.3 curve analysis | **never started**, dropped deliberately — see below | — |
| 2.4 AP Physics 1 mechanics | merged; #24 and #29 closed as folded | 23 relations in `relations/physics.py` |
| 3.1 pulled forward: definite integrals | merged; #30 closed as folded | `gexpr_integrate()` and `integr_poly3` |
| 3.2 exact linear algebra | merged, #59 and #60 | `runtime/matrix.c`; typed `det`/`inv`/`rank`/`solve` on exact rationals |
| 3.3 the rest of AP Chemistry | merged, #61 | `relations/chemistry.py`, 29 relations |
| 3.4 the rest of thermodynamics | merged, #62 | `relations/thermo.py`, 6 relations |
| 3.5 electricity and magnetism | merged, #62 | `relations/em.py`, 6 relations |
| 3.6 real wording data | **partly done**: three rounds shipped and are in the corpus; round four is parked on a branch, and hand-written accuracy was never made a gate | `gate.sh` gates generated phrasing and wrong relation only |
| Free-fall zero-height division by zero | merged; #26 closed as folded | the guard in `solvers_physics.c` |

**Wave 4, as it stands.** The map is `wave4-plan.md`. The three engine
workstreams are done or half done and everything they touch is gated:

- **4.1 symbolic integration** answers `integrate(x^2)` as `x^3/3 + C`, with
  powers, sums, constant multiples, `1/x` to `ln(x)` and sin/cos/exp of a linear
  argument. Anything else is refused rather than guessed. The gate differentiates
  the antiderivative and compares it against the integrand, which needs no table
  of expected answers and catches the failure this workstream produces: a rule
  off by a factor.
- **4.2 series** answers `series(exp(x),0,5)` and `taylor(sin(x),0,7)` as
  expressions about any centre. The gate compares the polynomial against the
  function it approximates and re-parses what it prints. Convergence tests are
  deliberately absent: a ratio test needs limits, and this engine has none.
- **4.3 differential equations** answers `ode(2y,0,1,1,64)` numerically with
  RK4, and `desolve(2y,0,1)` analytically as `exp(2x)`. The recogniser splits
  the right-hand side into an x factor and a y polynomial, so a rate that
  depends on x works too: `desolve(x*y,0,2)` is `2exp(x^2/2)` and
  `desolve(x/y,0,1)` is `sqrt(1+x^2)`. The two answers are checked against each
  other, which is stronger than either alone. A first-order linear equation with
  a non-zero x-dependent forcing term is refused, because its solution needs an
  integration by parts the rule table does not have.
- **4.4 waves, optics, fluids, modern physics** adds 34 relations and lands the
  catalogue at 136, in the units the exam states them: eV and nanometres for
  modern physics, because `h = 6.626e-34` cannot even be typed into this
  engine's number parser. Parity is 54,400 cases with no mismatch, and every
  relation carries the working line the step display prints.

- **4.5 interface** replaces the two message boxes with a screen model in
  `runtime/ui.c`: a menu, an input line with cursor editing, an answer pane that
  scrolls by whole rows with a bar saying where the window sits, eight pairs of
  history, three pages of keypad help, and the three pipeline failures told
  apart in words. `device/main.c` fell from 151 lines to 61 of key mapping.
  `test_screen` and `test_ui` drive it with synthetic keys and assert on the
  framebuffer, including that no render strays outside it.
- **4.6 graphing from the problem** is five renderers and one probe: plot the
  relation a call named with the answer marked on it, sweep one argument into a
  family, keep the decoder's top three candidates with their margins, draw the
  subject diagrams (force arrows, motion strobes, shaded integrals) and render a
  slope field. They hang off `runtime/graph.c` and `runtime/diagram.c`, each
  with a host test, and two keys reach them from the answer screen.

**The device half compiles, and that correction is worth recording.** The plan
said 4.5 and 4.6 could not be compiled without the Ndless toolchain. The runtime
path had been cross-compiled in CI all along, and `device/*.c` compiles for
`arm926ej-s` against the vendored SDK headers with nothing else installed, which
is how `expr.c` and then `diagram.c` were caught before they could break a link.
`crosscheck.sh` now compiles the device sources as part of the gate. What still
cannot happen here is *running* the result: a `.tns` needs Ndless's libraries and
a calculator, so every screen claim in this wave rests on host tests over a
320x240 framebuffer, not on a screen anyone has looked at.

**The catalogue did not outgrow the model after all.** Wave 3 closed at 102
relations with a worrying shape: hand-written accuracy flat at 72.6%, 73.8% and
72.3% across 84, 90 and 102 relations, while the generated gate fell 95.0%,
94.2%, 90.5%. That looked like a model out of capacity, and it gated wave 4.4
behind a decision about making the model wider.

The 34 relations went in and the fall did not continue. At 136 relations:

| | 102 relations | 136 relations |
| --- | --- | --- |
| generated held-out gate | 90.5% | **90.3%** |
| hand-written paraphrases | 72.3% | **71.8%** |
| fixed-point parity | 6/6 | **6/6** |
| full on-device path | — | 60/60 exact calls |

Both gates are within half a point, and the new relations are measured on
hand-written rows of their own — 1,942 eval phrasings across the whole
catalogue, not generated ones. So the wider model earns nothing at this size,
and the answer time it would cost stays unspent until someone times the device.

The gap that did show up was in the recipe rather than the model. The same
catalogue trained with the Makefile's old defaults — 6,000 steps, no paraphrase
mix — scores 83.6% and 32.7%. Those defaults are now the recipe the published
numbers came from, because an accuracy figure you cannot reproduce from `make` is
a claim rather than a measurement.

Wave 1's six code pulls verified together, with the trained weights present: the
combined tree builds, every C test passes, fixed-point parity is 6/6, the
tokenizer header matches, the Python suite is 36 passed and 1 skipped, and
`gate.sh` reports `ALL GATES PASS` including both accuracy gates. That tree was
the branch `capy/wave1-integrated`, which merged as #51; wave 2 followed through
#53, and waves 3.2 through 3.5 through #59–#62, so master now carries everything
this file tracks rather than an integration branch.

**2.3 was dropped deliberately.** Curve analysis in `graph.c` is the one slice of
this wave that adds no answering power: extrema and roots already appear on the
plot, and inflection points and monotonicity intervals are a display feature on a
calculator screen that already shows the curve. The same agent-day bought
differentiation and definite integrals, which answer questions the device
previously could not answer at all. If curve analysis comes back, it should come
back with a student-facing reason attached.

## Baseline measured on the build machine

The first time the recorded accuracy could be re-run rather than quoted, on the
pre-wave-1 tree (512-token vocabulary, single `solvers.c`), 1,000 held-out rows
through the whole on-device path:

| Measurement | Result |
| --- | --- |
| Exact call | 92.4% |
| Right relation, bad arguments | 5.6% |
| Wrong relation | 2.0% |
| Correct final answer | 92.5% |

The recorded figures were 91.6% exact and 2.9% wrong relation, so the claim
reproduces. On the wave-1 tree the same 1,000 rows give 92.5% exact and 1.9%
wrong relation, which says the decode mask does not cost accuracy — the
difference is inside the noise of a single sample.

## Catalogue v2 measured

84 relations — the 66 of wave 0 plus 8 mechanics and 10 chemistry and
thermodynamics — rebuilt as one atomic step with its own tokenizer, dataset and
retrain, on the same recipe (200,000 train rows, 6,000 steps, 512-token
vocabulary). Held-out rows through the whole on-device path:

| Measurement | 66 relations | 84 relations |
| --- | --- | --- |
| 2,000 rows, exact call | 92.5% | 92.9% |
| 2,000 rows, wrong relation | 2.2% | 2.5% |
| 2,000 rows, correct final answer | — | 93.0% |
| 1,000 rows, exact call (the gate) | 92.5% | 92.7% |

The exact-call difference is two questions in 2,000, inside run-to-run noise.
The honest reading is that eighteen more formulas answer at the accuracy the
catalogue already had, not that the model improved. Vocabulary: 167 merges from
`FIRST_MERGE=345`, weights still 5.91 MB, `reltable.h` and `bpe.h` regenerated.

## What the process has already taught us

- **`pr_create` commits untracked files**, so a `make` in the worktree sweeps a
  compiled binary into the pull request. Both the typed-answers and the kernel
  branch did this. Run `make clean` first, or add the binary to
  `stem-model/.gitignore`, which now lists every test binary and `bench_kernel`.
- **The CI ARM lane compiles a fixed file list** in `runtime/crosscheck.sh`. A
  new source file on the device path must be added there or the link fails with
  undefined references. This is how `expr.c` was caught, and it is why the
  script now says so in a comment.
- **Every workstream touches the Makefiles.** It is the one file no ownership
  split can keep apart, so the merge that matters is always the object list.
  Resolve it once, at integration, by taking the union.
- **A relation name is a numbered token, and the merge table starts right after
  the last one.** `FIRST_MERGE = REL_BASE + len(RELATIONS)`, so inserting a
  single relation name renumbers every BPE merge, invalidates `bpe.h` and makes
  the trained weights decode to nonsense. Growing the catalogue is therefore
  atomic with rebuilding the tokenizer, regenerating `data/`, retraining and
  regenerating both headers: one pull per batch, never one pull per relation.
  Regenerating `reltable.h` alone is worse than leaving it stale, because it
  then names merge ids as relations.
- **Three generated artefacts go stale, not one.** `bpe.h` (checked by
  `check_tokenizer_sync.py`), `reltable.h` (checked by
  `export_relations.py --check`) and the tokenizer fixture
  `runtime/fixtures/bpe_cases.txt` (checked by `gen_bpe_cases.py --check`, added
  in wave 2). The fixture is the nastiest: a stale one still passes on the old
  vocabulary and fails on the new one with a message that reads like a broken
  encoder, so it looks like a bug in the code rather than in the fixture.
- **A gate exemption that names an entry point rots silently.** `crosscheck.sh`
  exempted the runtime's one `double` by the old single entry point
  `solvers_solve`. The wave-0 split renamed that to `solvers_math_solve`, so the
  exemption stopped matching and the ARM lane began failing on code nobody had
  touched — invisibly on any machine without `arm-none-eabi-gcc`, which is every
  machine here. It now names `solve_exp` and marks it `noinline`. An exemption
  should name the narrowest thing that exists, not the widest thing that
  happens to contain it.
- **A measured win can be much smaller than the model of it.** The kernel floor
  argued for up to 2x; the measurement says the loop was already near its
  instruction count per MAC, so the real gain is about 3% per MAC and 1.12x on
  host timing. Quote the measurement, never the model.

## Agent configuration

Every subagent runs DeepSeek V4.1 Flash on high reasoning, billed to the
OpenCode key rather than Capy credits, in a worktree on the owner's own machine.
That model must be enabled in Settings → Models before an agent starts; a start
otherwise fails with an explicit "turned off" message and nothing to retry.

## Reporting template

Each wave close, and each workstream landing, reports:

- **Wave and workstream** — what was being built.
- **What a student can now do** — the capability in their terms, not the module names.
- **What got faster or safer** — the number that moved, and from what to what.
- **What was verified and what was not** — the command, the output, and the gap.
