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
| Typed expressions with no model (ahead of wave 1, closed 1.4's gap) | open, CI failing on the ARM lane until the crosscheck list was fixed | #15 |
| 0.1 split the solver and formula tables by subject | open, move-only, tests byte-identical | #19 |
| 0.2 build the artifacts | done on the build machine: dataset, 6,000-step checkpoint, `weights.bin` 5.91 MB, fixed-point parity 6/6 | — |
| 0.3 record the baseline | done, numbers below | — |
| 1.1 invalid output blocking | open, review fixes applied | #20 |
| 1.2 faster multiply-accumulate | open, honest size of win is ~3% per MAC | #21 |
| 1.3 vocabulary 1,024 | open, plumbing closed in the same branch | #18 |
| 1.4 typed maths without the weights file | open, unbuildable here so unverified | #17 |

All five code pulls verified together, with the trained weights present: the
combined tree builds, every C test passes, fixed-point parity is 6/6, the
tokenizer header matches, the Python suite is 36 passed and 1 skipped, and
`gate.sh` reports `ALL GATES PASS` including both accuracy gates.

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
