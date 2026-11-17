# Tier 1 plan: the courses the catalogue already mostly serves

Tier 1 is AP Precalculus, AP Calculus AB, AP Calculus BC, SAT/ACT numeracy, and
intro high-school physics and chemistry. These are the courses whose questions
are already within reach: a number or a short exact expression, produced by a
formula the model can pick from words, or by the typed engine with no model at
all.

This page is the execution plan for that tier. `docs/waves.md` stays the plan of
record for waves; this is the plan for one of the course tracks they feed.
Read it before starting a phase, and update the status table when a phase
changes state.

## Four facts that constrain every phase

1. **A relation add is atomic with a retrain.** Relation names are numbered
   tokens and the BPE merge table starts right after the last one, so inserting
   one name renumbers every merge, invalidates `bpe.h`, and makes existing
   weights decode to nonsense. `bpe.h`, `reltable.h` and
   `runtime/fixtures/bpe_cases.txt` go stale together. One batch, one retrain,
   never one relation at a time.
2. **For AB and BC the lever is the typed engine, not the model.** Today the
   parser and evaluator already answer expressions, `d/dx`, definite and
   indefinite integrals, Taylor series, first-order ODEs, and exact rational
   `det`/`inv`/`rank`/`solve`. Missing are limits, Riemann sums, average value,
   tangent lines, by-parts, partial fractions, improper integrals, convergence
   tests, arc length, and polar/parametric calculus. All of those are code, and
   none of them needs a retrain.
3. **A claim is a command.** Every phase closes with the check that proves it,
   quoted with its output. A check that could not run is reported as not run.
   The project already treats an unrun gate as a defect, not a detail.
4. **Verification, not authoring, is the cost.** A relation is about 100 lines
   across the Python contract, the C solver, the parity vectors, the phrasing
   bank and the step template. What takes the time is proving the C and Python
   agree on every generated row and that the wording corpus is honest.

## The ladder

| Phase | What it delivers | Retrain | PRs | Owner |
| --- | --- | --- | --- | --- |
| P0 | Ground and inventory: the course-to-relation matrix as a tool, not prose | no | 1 | coordinator |
| P1 | Per-course measurement: course tags, per-course floors in `gate.sh` | no | 1 | worker |
| P2 | Typed calculus engine: limits through polar area | no | 4 | worker |
| P3 | Relation batch A: precalculus and AB wording | batch | 2 | worker |
| P4 | Relation batch B: SAT/ACT numeracy and units | batch | 1 | worker |
| P5 | Relation batch C: intro physics and chemistry | batch | 2 | worker |
| P6 | The retrain: regenerate, train, export, measure per course | yes | 1 | coordinator |
| P7 | Course surface: picker, formula sheet, practice prompts | no | 2 | worker |
| P8 | Close the tier in outcomes a student can act on | no | 1 | coordinator |

Estimates are in PR-sized units and honest about wall clock: with three or four
workers running disjoint phases, P0 through P5 is two to three days, P6 is an
evening of training, and P7 is another day. The retrain is 70 minutes on the
Mac mini and cannot be parallelised.

## P0 — Ground and inventory

**Goal.** Turn "what does this course need" into a table a script produces, so
later phases and later courses cannot drift into opinion.

**Deliverable.** `stem-model/course_audit.py` plus `stem-model/courses.py`, the
data it reads: each course, its units, and the exam topic behind each unit,
mapped to a relation, a typed command, or a gap. It prints, per course, how
many topics are served, how many are typed, and how many are missing, and exits
non-zero when a served claim names a relation that does not exist. The script
lives beside the other analysers rather than in the root `tools/` directory,
which holds repository-level scripts.

**Exit gate.** `python3 stem-model/course_audit.py` prints the matrix and the
counts per course, and the matrix matches `RELATIONS` exactly — a relation
listed as serving a topic that is not in the registry fails the run.

**Why first.** It is the acceptance list for everything after it. It also stops
the plan from quietly becoming "the relations I felt like writing".

## P1 — Per-course measurement

**Goal.** Make each course a number before any of them grows, so a regression
has somewhere to show up.

**Deliverables.**

- `courses.py` holds one `exact_floor` per course, with the row count and the
  date it was measured. The map is already the single source: a relation's
  courses come from `courses_for()`, so nothing is duplicated.
- `report_relations.py --course NAME`, printing the same table scoped to one
  course, and `--course-floor`, which scores the corpus once and prints every
  course against its own floor.
- `gate.sh` gains `per-course floor`, inside the accuracy branch with the
  existing gates. The budget is the measured number minus `FLOOR_SLACK`, and a
  course under `MIN_ROWS` scored rows is reported as too thin to judge rather
  than judged anyway — eleven questions is a coin flip wearing a decimal point.

**Exit gate.** `sh stem-model/gate.sh quick` still reports `ALL GATES PASS`, and
`report_relations.py --course-floor` prints the five lines. Those numbers are
recorded in this file under "Baseline", dated and tied to a commit.

**Failure policy.** A floor that fails on its first run is a budget written too
high, not a broken model: fix the budget to the measured number in the same
commit, with the measurement in the message.

## P2 — Typed calculus engine (no retrain)

**Goal.** Cover what AB and BC ask for that needs no model at all. This is the
cheapest capability per line in the whole tier.

Four PRs, each under the limit and each owning distinct files:

| PR | Adds | Owns |
| --- | --- | --- |
| 2a | `limit(f, x, a)` by sampling with a stated tolerance and a refusal when the two sides disagree; `tangent(f, a)` as a line in point-slope form; average rate of change over an interval | `runtime/expr.c`, `runtime/test_expr.c` |
| 2b | `riemann(f, a, b, n)` with left, right, midpoint and trapezoid; `avg_value(f, a, b)`; `arc_length(f, a, b)` via the integral of `sqrt(1 + f'^2)` | `runtime/expr.c`, `runtime/test_symbolic.c` |
| 2c | Integration technique: by-parts for products with a polynomial factor, partial fractions for distinct linear factors, improper integrals as a limit of the bound; refusal otherwise | `runtime/expr.c`, `runtime/test_symbolic.c` |
| 2d | Convergence for the series the engine already builds: geometric, ratio test on polynomials and factorials, alternating error bound; polar and parametric area and arc length on the existing parse tree | `runtime/expr.c`, `runtime/test_series.c` |

**Exit gate.** The four test binaries pass, `sh runtime/crosscheck.sh` compiles
the new code for `arm926ej-s` (or reports 77 with the reason), `gate.sh` is
green, and every new form prints a refusal — not a guess — where it does not
apply. A CAS that answers `integrate by parts` for something it cannot do is
worse than one that says no.

**Deliberate limit.** No general symbolic simplification, no trig substitution
beyond the identities already present, no series convergence for anything that
needs a real limit engine. Those are wave-6 K21 and K24, and lying about them
here would cost the refusal behaviour that makes the rest trustworthy.

### Improper integrals were attempted, and refused by the engine

An infinite tail was tried twice and dropped, and the reason belongs in the plan
because it is a property of the engine rather than of the effort.

The textbook substitution `x = a + t/(1-t)` is correct and unusable here: near
`t = 1` the Jacobian `1/(1-t)^2` passes 32767 and saturates in Q16, so every
tail refused. Walking the bound outward instead, in eight-unit pieces, was
accurate per piece — and wrong in aggregate, because the adaptive integrator
returns its own absolute floor (about 16 Q16 units) for a piece whose true
integral is below it. Summing four hundred such pieces produced 1.085 for
`intinf(1/x^2,1)` and still climbing, which is a plausible wrong answer, the one
thing this project refuses to print.

So `intinf`/`intninf` are not shipped. Answering them honestly needs either a
wider intermediate than Q16 or an asymptotic rule table per integrand family;
both are engine work, and neither belongs in a slice whose contract is "no
retrain, four small PRs".

## The batch order changed after P2

P2 measured something the plan had assumed otherwise: AB and BC are almost
entirely *expression-shaped*. "Find the average rate of change of f over [2, 5]"
is an expression plus bounds, which the typed engine now answers; it is not a
fixed-arity call with numbers from a sentence. So batch A is much smaller than
written, and the relations that genuinely need the model are the numeric,
word-problem-shaped ones.

The order is therefore **batch B (SAT/ACT numeracy) before batch A (precalculus
and AB wording)**, and batch A shrinks to the precalculus models that really are
number-shaped. P4's numbering stays as written so the earlier references still
resolve; the execution order does not.

## P3 — Relation batch A: precalculus and AB wording

**Goal.** Cover the course topics that are formulas with words, where the typed
path cannot help because the numbers come from a sentence.

Candidate set, to be finalised by P0's gap list: `rational_asymptote`,
`inverse_from_table` (refused if it cannot be exact), `exp_growth_model`,
`log_model`, `related_rates`, `tangent_line_from_text`, `implicit_point`,
`optimization_text`. Roughly eight.

**Owns.** `relations/precalc.py`, `relations/calculus.py`,
`runtime/solvers_math.c`, `runtime/test_solvers.c`, `fixtures/paraphrase_math_r4.jsonl`.

**Exit gate.** `./test_solvers | grep "0 mismatches"`, `python3
export_relations.py --check`, `python3 export_steps.py --check`, `python3
check_steps.py`, and `python3 evaluate_paraphrase.py --check-corpus` accepting
every new row. A relation whose solver is not exact is cut, not fudged.

## P4 — Relation batch B: SAT/ACT numeracy and units

**Goal.** The highest-audience wording in the tier: percentages, ratios,
rates, averages, geometry, simple probability.

Candidate set: `percent_change`, `percent_of`, `ratio_share`,
`average_speed`, `combined_rate`, `simple_interest`, `rectangle_area`,
`triangle_area`, `circle_measure`, `volume_prism`, `probability_single`.
Roughly ten.

**Owns.** `relations/units.py`, `runtime/solvers_units.c`,
`runtime/test_solvers.c`, `fixtures/paraphrase_units_r4.jsonl`.

**Exit gate.** As P3, plus one addition: the phrasing bank has to be written in
exam register, not textbook register — "A shirt is marked down 30% from $45"
rather than "A quantity decreases by 30 percent". That register is the thing
being tested, and P6 measures whether it worked.

**Status, first slice.** Five of the eleven candidates are in:
`percent_change`, `percent_of`, `ratio_share`, `average_speed`,
`simple_interest`. Each has the Python contract, an eleven-phrasing exam-register
bank in `surface/units.py`, a C solver, parity vectors, a step template and a
distractor line. The exam-register corpus is
`fixtures/paraphrase_units_r4.jsonl` (40 eval rows, every row solvable and every
argument grounded in its question).

Verified on the slice: `./runtime/test_parity` reports 3384 cases and 0
mismatches with 0 gaps, so the C and Python solvers agree on every generated
row; `check_steps.py` reports 128/128 templates agreeing with their solvers;
`evaluate_paraphrase.py --check-corpus` accepts the new corpus.

The other six candidates do not land in this slice, and the reason is
mechanical rather than editorial: adding a relation renumbers the tokenizer, so
every relation add has to be atomic with regenerating `bpe.h`, `reltable.h` and
`fixtures/bpe_cases.txt` and with a retrain. One slice is one training cycle.
Coordinate workers for the rest are blocked on organization credits, and the
authoring is serial until that clears.

## P5 — Relation batch C: intro physics and chemistry

**Goal.** The formulas an intro course asks for that are missing: collisions,
pulleys, series circuits, limiting reagent, titration, empirical formula.

Candidate set: `inelastic_velocity`, `elastic_velocity`, `collision_energy_loss`,
`atwood_accel`, `incline_accel`, `series_resistance`, `circuit_power`,
`limiting_reagent`, `percent_composition`, `titration_molarity`,
`empirical_formula`. Roughly eleven.

**Owns.** `relations/physics.py`, `relations/chemistry.py`,
`runtime/solvers_physics.c`, `runtime/solvers_chem.c`, `runtime/test_solvers.c`,
`fixtures/paraphrase_physics_r4.jsonl`, `fixtures/paraphrase_chem-units_r4.jsonl`.

**Exit gate.** As P3. `runtime/crosscheck.sh` also has to accept the new code,
and `runtime/Makefile`'s explicit object list is the file every phase touches:
resolve it once at integration by taking the union.

## P6 — The retrain

**Goal.** One catalogue change, measured per course.

Two retrains, not three: batches A and B together (math and numeracy share a
corpus stream), then batch C. A retrain that had to be bisected twice was a
batch that should have been split, and two is the compromise between isolating
a regression and spending three evenings on training.

**Procedure, in order.** `make data` with the new catalogue and the new
phrasings; `make train` (12,000 steps, seq 192, batch 32) on the Mac mini;
`export_model.py` and `export_tokenizer.py`; `check_tokenizer_sync.py`;
`./test_fixed ../weights.bin`; `evaluate.py` and `evaluate_device.py` on held-out
rows; `report_relations.py` overall and per course.

**Exit gate.** Fixed-point parity 6/6, both accuracy gates pass, every
per-course floor reports its number, and this file records before and after for
all five courses. If generated accuracy rises while hand-written accuracy falls,
that is a regression to report, not a trade to celebrate.

**Status: two cycles run.** Both on the Mac mini, 12,000 steps, seq 192, batch
32, 4.2-4.3 ks each. Cycle one is batch B's first five relations; cycle two adds
the wording families in `surface/vary.py` and nothing else.

| cycle | relations | wording families | held-out templates | wording never trained | precalc | SAT/ACT | intro |
| --- | --- | --- | --- | --- | --- | --- | --- |
| baseline before batch B | 136 | no | - | - | 61.6% | 67.2% | 64.2% |
| one: five numeric relations | 141 | no | 94.8% | - | 64.3% | 74.1% | 70.1% |
| two: + wording families | 141 | yes | 93.0% | 98.9% | 64.5% | 72.4% | 68.5% |

Reading it honestly: cycle one is the course win, roughly 70 more correct
hand-written questions. Cycle two keeps every floor passing but gives back 1.7
points on SAT/ACT and 1.6 on intro against a 460- and 752-row sample, which is
inside the sampling noise rather than a demonstrated cost. What cycle two buys is
measured directly instead: on 1,041 rows whose wording never appears in training
("per cent", "Work out") it is 98.9% exact, against 98.5% on the same templates
in ordinary wording. Wording novelty stops costing accuracy, which was the point
of the cycle.

Fixed-point parity is 6/6 in both cycles (worst maxdiff 0.0595 and 0.0533).

**The variance result, and why every number above needs a second seed.** A third
cycle (batch B's remaining five relations) scored 89.4% on the same 500 held-out
rows, 3.6 points below cycle two, with the course floors down with it. Before
either believing or patching that, the same training data was retrained with
`--seed 2`: **93.0%**, matching cycle two exactly and recovering the floors.

| run | data | seed | held-out templates | reserved wording | precalc | SAT/ACT | intro |
| --- | --- | --- | --- | --- | --- | --- | --- |
| one | +5 numeric | 1 | 94.8% | - | 64.3% | 74.1% | 70.1% |
| two | + wording families | 1 | 93.0% | 98.9% | 64.5% | 72.4% | 68.5% |
| three | +5 geometry and rate | 1 | 89.4% | 98.2% | 60.5% | 69.6% | 65.3% |
| three' | same as three | 2 | 93.0% | 98.1% | 63.3% | 71.1% | 69.4% |

So the run-to-run spread on a 500-row probe is **±3.6 points**, wider than every
difference between the cycles except cycle one's SAT/ACT gain. Two consequences,
both binding on how this document reports from here:

1. A single 500-row probe cannot separate two nearby configurations. Any claim
   that a catalogue or wording change helped needs either the within-model
   contrast (reserved wording against plain wording, same weights) or at least
   two seeds per arm.
2. Cycle one's +6.9 points on SAT/ACT is bigger than the spread and stands. The
   1.7-point give-back attributed to the wording families in cycle two does not:
   it is inside the noise floor, and this file should not have implied otherwise
   before the control ran.

Fixed-point parity is 6/6 in every cycle (worst maxdiff 0.0595, 0.0533 and
0.0327).

**The vocabulary squeeze, measured.** The 512-entry vocabulary splits as 256 base
tokens + one special per relation + whatever is left for BPE merges, so
`merges = 251 - relations`. At 141 relations there were 110 merges, at 146
there were 105, at 150 there were 101. Tokenising the same 249,742 rows at 640
instead of 512 gives **mean 51.4 tokens instead of 62.0 and max 148 instead of
182** - 17% shorter sequences and 34 tokens of headroom back - which is the
compression the growing catalogue had been eating.

**The recipe is unstable.** Batch A's two seeds, on identical data, scored
75.8% and 90.8% on the same 500 held-out rows. Fifteen points, four times the
spread measured a cycle earlier, and not attributable to the new relations:
both seeds fail the same handful, but the bad seed additionally misses 112 rows
the good one gets. Until that is fixed, every accuracy number in this file is a
sample from a wide distribution rather than a property of the catalogue.

**Argument order is a learnability decision.** `average_rate_of_change` was
authored as `(f(a), f(b), a, b)` while its sentence reads `f(1) = 3 and
f(5) = 11`, so the call required reordering numbers rather than copying them
left to right. It failed all four held-out rows under both seeds. It is now
`(a, f(a), b, f(b))`, giving the call `1 3 5 11`. Every relation in the
catalogue should be checked against its sentences for this.

**The 640-entry vocabulary, measured.** Retraining the same catalogue at 640
entries turns the run that scored 75.8% at 512 into **90.2%** on the same 500
held-out rows, with reserved-wording accuracy at **98.4%** instead of 82.3%. The
bad basin the 512 vocabulary could fall into - a 15-point swing between seeds -
is gone at 640, which is consistent with the merge starvation: 101 merges at 512
against 226 at 640, and a mean of 62.0 row tokens against 51.9.

Both seeds at 640, finished and scored on the same 500 rows: **90.2% and
93.6%** exact, with reserved-wording accuracy at **98.4% and 98.6%**. The spread
is 3.4 points, back at the level measured before the merge starvation, against
the 15 points the same pair produced at 512.

The ceiling did not move. 90.2-93.6% at 640 straddles the best 512 run (90.8%),
so the vocabulary fixed the instability and left the accuracy ceiling where it
was. Whatever sets that ceiling is upstream of tokenisation - most likely the
12,000-step, single-learning-rate recipe - and it is the next thing to measure
rather than guess at.

## P7 — Course surface

**Goal.** A student who owns AP Physics C should not have to guess what the
program knows.

**Deliverables.** A course picker backed by the formula catalogue that already
exists as data (wave 6 K33), unit-aware argument prompts on the picker (S32),
and a practice page that renders the phrasing bank as fill-in-the-blank
questions (W26). Two PRs: the picker and data, then the practice page.

**Exit gate.** `test_ui` and `test_screen` cover the new screens with synthetic
keys and assert on the framebuffer, including that nothing renders outside it.

## P8 — Close the tier

Report per course in outcomes, not modules: what a student can now ask, which
number moved, from what to what, and what was measured on which split. The
template at the bottom of `docs/waves.md` is the shape.

### The tier, closed

**What a student can now ask, by course.** Every number below is exact-call
accuracy on hand-written rows, measured through the int8 C engine.

| Course | Before P3 | After P3-P5 | Floor | What changed for this course |
| --- | --- | --- | --- | --- |
| AP Precalculus | 61.6% (326/529) | 64.5% (341/529) | 61.6 | average rate of change, vector addition, dot product and scaling |
| SAT / ACT maths | 67.2% (309/460) | 72.4% (333/460) | 67.2 | percentages, percent change, ratios, rates, interest, areas, volumes, probability |
| Intro physics and chemistry | 64.2% (483/752) | 68.5% (515/752) | 64.2 | collisions, series resistance, limiting reagent |
| AP Calculus AB | 69.6% (32/46) | 71.7% (33/46) | not judged, 46 rows | the typed engine, not relations: limits, area between curves, volume of revolution |
| AP Calculus BC | no rows | no rows | not judged | polar area and parametric arc length are typed-engine work |

**What the catalogue covered at 153 relations.** These topic counts are the
measurement made at that size; the catalogue is 171 today, and the eighteen
chemistry relations that landed in #117 are not reflected in them. At 153,
intro physics and chemistry answered all 42 of its topics, AP Precalculus 29
of 31, SAT/ACT 28 of 32, AP Calculus AB 11 of 20. Nineteen topics across the
five courses remain open, and every one of them is analytical rather than
numeric: asymptotes, polar graphing, tangent lines, chain rule inside a word
problem, implicit differentiation, related rates, optimisation, straight-line
motion, critical points, Mean Value Theorem, Riemann sums, series convergence
and error bounds.

**What was measured on which split.** The course numbers above are the
hand-written corpus, which is what a student types. The generated split reads
higher and is not the headline: 90.2% and 93.6% exact on 500 held-out phrasing
rows at the 640-entry vocabulary, with 2.2-3.2% wrong relation. Wording the
model has never seen reads 98.4% and 98.6% exact against 98.5% on ordinary
wording from the same weights, which is the one claim immune to seed variance.

**What a student can reach.** The picker opens on their course, lists that
course's callable topics, and never offers a gap or a typed-engine topic as a
call. The subject route stays for anyone who does not know their course.

**What is not in the tier, deliberately.** The nineteen analytical topics above;
`circle_measure`, cut because pi r^2 has no exact rational answer; the
by-parts and improper-integral rules (P2c), which the typed engine already
answers numerically; and AP Statistics, AP Physics C, multivariable calculus and
every course whose answer is a derivation rather than a number.

**The one number that is not closed.** The accuracy ceiling is 90-94% on
unseen phrasings at the 153 relations of that measurement, and it did not move
when the vocabulary stopped starving itself. That ceiling is a training-recipe
problem, not a catalogue problem: more relations will not raise it, and the
recipe is where the next tier should start.

## Ownership matrix

Two workers never edit one file in the same phase. Shared files, resolved at
integration:

| File | Phases that touch it |
| --- | --- |
| `runtime/expr.c` | P2 only, one PR at a time |
| `runtime/test_solvers.c` | P3, P4, P5 (append-only per batch) |
| `runtime/Makefile` | P2, P5 (object list; take the union once) |
| `stem-model/Makefile` | P6 only |
| `gate.sh` | P1 only |
| `docs/waves.md`, this file | coordinator only |

## Risks, with what makes each survivable

| Risk | Mitigation |
| --- | --- |
| A relation cannot be exact | Cut it. Wave 4 cut three for numeric range; the precedent is set and the refusal is the product |
| The per-relation floor is already at 13 against a budget of 12 | Re-measure before and after each retrain, and never move a budget without the measurement in the same commit |
| Word-problem accuracy is ~71% on hand-written rows and SAT wording is a new register | The corpus is the deliverable, not the relations; P4's exit gate names the register |
| Only one machine can train | Authoring runs while the mini trains; the retrain is the last step, not the first |
| A stale generated artifact passes locally | Three artifacts are checked by three scripts in `gate.sh`; run them before every retrain |
| Copyright on exam wording | Raw exam text stays in a gitignored local corpus; only generated or paraphrased rows are committed |

## Baseline

Measured 2026-11-16 on the corpus's eval split (1,942 hand-written rows) with
the weights trained from `edbe24a6`, through `runtime/main_device`:

| Course | Rows | Exact call | Floor |
| --- | --- | --- | --- |
| AP Precalculus | 529 | 61.6% | 61.6 |
| AP Calculus AB | 46 | 69.6% | not judged: below the 60-row minimum |
| AP Calculus BC | 0 | — | not judged: no corpus rows, BC is typed-engine work |
| SAT / ACT maths | 460 | 67.2% | 67.2 |
| Intro high-school physics and chemistry | 752 | 64.2% | 64.2 |

*Corrected 2026-11-17:* the eval split is **2,150 hand-written rows across 171
relations**, not the 1,942 this baseline was measured on. Three changes moved
under it: the r4 wording batches landed after the baseline was recorded, the
eighteen AP Chemistry relations arrived in #117, and the eval fixture was
rebuilt from full-text stems when the 160-character cap in `apchem_audit.py` was
found. These are counts, not measurements. No accuracy figure above, below, or
in the earlier form of this note was re-measured on the 2,150-row split: the
table's percentages stay as measured on 1,942 rows, and the floor comparison
that stood here was made against the 2,078-row split and is not restated.

Two things the table says that a headline number could not. AP Calculus BC has
no corpus rows at all, because the only calculus relations are `deriv_poly3` and
`integr_poly3` and both are AB's: BC is served by the typed engine, and its
accuracy is judged by `test_expr`, `test_symbolic` and `test_series` rather than
by wording. AP Calculus AB has 46 rows, which is under the minimum, so it is
reported and not judged until P3 gives it relations of its own.

The aggregate the project has quoted until now — 71.2% on the same 1,942 rows —
is higher than every course here because it also counts relations that serve no
Tier 1 course. Both numbers are true; this table is the one a student in one of
these courses experiences.

## Status

| Phase | State | Evidence |
| --- | --- | --- |
| P0 inventory | done | `python3 stem-model/course_audit.py`: 5 courses, 29 units, 134 topics, 19 gaps, every relation claim registered |
| P1 per-course measurement | done | `report_relations.py --course-floor`: three PASS, two too thin to judge; `gate.sh` carries the line |
| P2a limits, tangents, average rates | done, #82 | `test_expr` covers the four answers and five refusals; `limit` accepts only when the two-sided gap closes at the rate the offset does |
| P2b Riemann sums, average value, arc length | done | `riemann` with mid/left/right/trap, `avgvalue`, `arclen`; `gate.sh quick` green |
| P2c by-parts, partial fractions, improper | **not shipped** | definite integrals of those integrands already answer numerically; the symbolic rules need a rewrite, and improper tails hit the integrator's own floor (above) |
| P2d polar area, parametric arc length | done | `polarea`, `parclen`; convergence tests stay with wave-6 K24, which needs a limit engine |
| P3 batch A relations | done, #97 | `average_rate_of_change` and three vector relations; parity 3,672 cases with 0 mismatches and 0 gaps |
| P4 batch B relations | done, #89 and #94 | ten relations across the numeric and geometry slices; `circle_measure` cut because pi r^2 has no exact rational answer |
| P5 batch C relations | done, #97 | `collision_velocity`, `series_resistance`, `limiting_moles`; intro physics and chemistry reaches zero gaps (42 of 42 topics) |
| P6 retrain and per-course measurement | done, with a caveat | four cycles measured; the course floors recorded above come from the 146-relation cycle, and the 153-relation cycle is measured on held-out and wording rows only because the floors need a corpus-wide run |
| P7 course surface | not started | shape pending: `runtime/ui.c` already carries a subject -> relation -> arguments flow, so a course picker is closer to scoping that flow than to a new screen |
| P8 closing pass | in progress | relation counts corrected in `status.md` and `ROADMAP.md`; the outcome report waits for P7, because a tier closed without its main surface would be a row that overstates |

## What is not in Tier 1

AP Statistics (needs a data-row input type), AP Physics C (separate batch,
previous tier), multivariable calculus, linear algebra beyond 3×3, second-order
differential equations, and every course whose answer is a derivation rather
than a number.
