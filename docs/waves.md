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
| 4.3 | **Differential equations** — separable, first-order linear, logistic, RK4 | Cooling, kinetics, RC, populations | `runtime/expr.c` |
| 4.4 | **Waves, optics, fluids, modern physics** | The rest of AP Physics 2 | `relations/waves.py`, `runtime/solvers_waves.c` |
| 4.5 | **Interface** — keypad templates, step display, error explanations, history | A correct answer nobody believes is not a product | `device/*.c` |

## Status

`open` means a pull request exists and is verified locally; `merged` means it is
on master. Nothing here is called done before it is merged.

| Workstream | State | Pull request |
| --- | --- | --- |
| Typed expressions with no model (ahead of wave 1, closed 1.4's gap) | open, green once the crosscheck fix reached the integration base | #15 |
| 0.1 split the solver and formula tables by subject | open, move-only, tests byte-identical | #19 |
| 0.2 build the artifacts | done on the build machine: dataset, 6,000-step checkpoint, `weights.bin` 5.91 MB, fixed-point parity 6/6 | — |
| 0.3 record the baseline | done, numbers below | — |
| 1.1 invalid output blocking | open, review fixes applied | #20 |
| 1.2 faster multiply-accumulate | open, honest size of win is ~3% per MAC | #21 |
| 1.3 vocabulary 1,024 | **closed as a measured regression**: 87.9% exact against 92.5% at 512 on the same 2,000 rows. The capacity bug it exposed is a separate open pull | #18 closed, #23 open |
| 1.4 typed maths without the weights file | open, stacked on #15 | #17 |
| 2.1 solve for any variable (equation engine) | open; three math relations routed through it with `test_solvers` byte-identical | #25 |
| 2.2 differentiation | open; typed `d/dx`, `diff`, `derivative`, worst finite-difference disagreement 15 permille | #27 |
| 2.3 curve analysis | **not started**, and the wave closed without it — see below | — |
| 2.4 AP Physics 1 mechanics | 8 relations landed with the retrain; the solver/relation parity gate shipped separately, because a checker is reviewable without a vocabulary rebuild | #24, #29 |
| 3.1 pulled forward: definite integrals | open; `integrate(x^2,0,3)` answers 9, worst relative error 1.08e-4 | #30 |
| 3.2 exact linear algebra | open, stacked on #58; typed `det`/`inv`/`rank`/`solve` on exact rationals, 4x4 cap, 47 host cases, device cross-compiles (+3.5 KB text) | #59 |
| 3.3 and 3.4 pulled forward: chemistry and thermodynamics | 10 relations landed with the retrain | #29 |
| Free-fall zero-height division by zero | open, 5 lines, reproduced under `-fsanitize=undefined` before the fix | #26 |

Wave 1's six code pulls verified together, with the trained weights present: the
combined tree builds, every C test passes, fixed-point parity is 6/6, the
tokenizer header matches, the Python suite is 36 passed and 1 skipped, and
`gate.sh` reports `ALL GATES PASS` including both accuracy gates. That tree is
the branch `capy/wave1-integrated`, and every wave 2 pull is based on it,
because master cannot take wave 2 until wave 1 merges.

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
