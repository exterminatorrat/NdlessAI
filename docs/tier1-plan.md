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
| P0 inventory | done | `python3 stem-model/course_audit.py`: 5 courses, 29 units, 128 topics, 33 gaps, every relation claim registered |
| P1 per-course measurement | done | `report_relations.py --course-floor`: three PASS, two too thin to judge; `gate.sh` carries the line |

## What is not in Tier 1

AP Statistics (needs a data-row input type), AP Physics C (separate batch,
previous tier), multivariable calculus, linear algebra beyond 3×3, second-order
differential equations, and every course whose answer is a derivation rather
than a number.
