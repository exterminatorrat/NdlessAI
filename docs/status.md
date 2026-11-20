# Status

Where the project stands, what was planned, and what is left. Read this first
when resuming work; it is the map, and `stem-model/PROGRESS.md` is the
per-relation ledger.

Last updated: 2026-11-18, at commit `cc4b8505`. Every figure below names the set
it was measured on, because two figures with the same name and different sets
cannot be compared — and this page has already carried one that no tree on the
machine could reproduce.

## One paragraph

The system reads an English word problem, a 5.2M-parameter model picks one of
171 exact solvers, and C code computes the answer. Waves 1 through 3 are on
master. Two seeds per arm, measured on 2026-11-18: **89.8% exact on the 2,000-row
generated held-out gate** (90.0 and 89.6), and **70.9% exact on the 2,150-row
calibrated hand-written split** (72.2 and 69.6), both through `runtime/main_device`.
Every course floor passes on the rows it was calibrated on. Four wording and
argument-selection ideas were measured and closed the same day — corpus
composition, minimal-pair contrast rows, register breadth and slot-cue
selection — so the remaining levers are capacity and the domain gap, not
phrasing (`docs/closed-lines.md`). On the real College Board AP Chemistry bank
the app names a correct call for **1 of 19 readable rows**, and the bank's own
shape caps trustworthy single-call rows at **28 of 1,136**; that limit is
multi-step questions, not wording. The device app builds to `stemhelp.tns`, and
the one thing never done is running it on a physical calculator.

## What works today

| Capability | State | Evidence |
|---|---|---|
| 171 relations, Python reference | done | `sh stem-model/gate.sh` |
| 171 relations, C solvers | done | eleven per-subject units; `test_solvers` reports 14,180 assertions on every run |
| Model picks the right tool | done, and the current limit | 70.9% exact on the 2,150-row calibrated hand-written split, r7 two-seed mean (72.2 / 69.6); 89.8% exact on the 2,000-row generated gate (90.0 / 89.6); both through `runtime/main_device`. Seed 1 of the r5 pair splits the same 2,150 rows as 74.0% exact, 15.3% right relation with wrong arguments, 10.7% wrong relation and 1 refusal |
| Course floors | done | each scores its own calibrated set: AP Precalculus 71.4% of 561 rows against a 61.6 floor, SAT/ACT maths 73.25% of 516 against 67.2, intro physics and chemistry 71.15% of 776 against 64.2, r7 two-seed means. A set the floor was not calibrated on is refused unless `--allow-drift` is passed |
| Grounding guard | done | refuses calls whose arguments are not in the question |
| Abstention on a close call | done | a relation margin under 500 fixed-point units answers "uncertain": 22 rows caught and 7 lost over the 4,300 hand-written rows (2,150 × 2 seeds), 3.1:1 at 76% precision; the 4,000-row generated control catches 1 and loses 1 |
| Honest refusals | done | four outcomes the screen tells apart, each asserted on the framebuffer: uncertain, arithmetic past the exact engine's node pool, arithmetic with no value, and weights whose vocabulary disagrees with the build's. The old screen blamed the tokenizer on every refusal |
| Typed expressions, no model needed | done | `integrate(x^2,0,3)` answers 9 and `d/dx`, `det`, `inv`, `rank`, `solve` answer on exact rationals; `integrate(x^2)` answers `x^3/3 + C`, `series(exp(x),0,5)` a Taylor polynomial, `ode(2y,0,1,1,64)` a numerical solution and `desolve(2y,0,1)` its closed form `exp(2x)`. Arithmetic the exact engine cannot hold, or has no value for, refuses by name instead of saturating to 32768 |
| Find a relation, fill its slots, reuse the answer | done | typing narrows the 171-name catalogue live; the relation's own formula sits above one prompt per argument; each prompt names the unit the phrasing bank states (`v0 (m/s)`, 124-row table in `runtime/unitstable.h`); `TAB` fills a slot with the previous answer. No model on this path, so it cannot be wrong |
| Function graphing | done | `test_graph`, merged in PR #8 |
| Device build | done | `stemhelp.tns` and `weights.bin.tns`, cross-compiled for `arm926ej-s` |
| On-calculator run | **not done** | no hardware in the build environment, and it needs Ndless on a supported OS |

## The honest limit on real exam questions

This is the one measurement the project has that the phrasing bank did not write,
and it is small on purpose.

- **The bank.** 1,136 multiple-choice items and 202 free-response prompts in
  1,159 lettered parts. **85 of 2,295 items are answerable today (3.7%)**, or 86
  of 2,079 (4.1%) against the judged denominator left after OCR resolved 89 of
  the 301 items whose options extraction had lost.
- **The frozen slice.** 630 held-out ids, pinned to a file. 28 of them carry a
  relation the catalogue can name, and **19 fit inside the 192-token window**;
  the other 9 run 197 to 354 tokens and are unmeasurable, not wrong.
- **The number.** **5.3% of those 19 rows** — one correct call in 19, averaged
  over the two seeds; the r5 pair read 2/19 on seed 1 and 0/19 on seed 2 — against
  70.9% on our own 2,150-row hand-written split. It is unchanged across the r5,
  r6 and r7 arms.
- **The cap.** A hand read of the 70 bank rows that survive a mechanical gate
  finds **28 trustworthy single-call rows of 1,136**, 25 of which fit the
  192-token window. No amount of wording work grows this measurement past 28
  rows, so the slice is a directional test rather than a resolved one.
  [`ap-chemistry-ceiling.md`](ap-chemistry-ceiling.md) counts what the rest of the
  bank would take to reach, and why 100% of it is not a goal this design can meet.
- **Why, and it is not wording.** 23 of those 70 rows need a second computation,
  9 name the wrong quantity, 6 need a tabulated constant the row does not carry
  and 3 have no relation at all; bank-wide, 835 of 1,136 items name no catalogue
  relation and 51 name two or more. The window costs 11% of the 28 single-call
  rows. **AP Chemistry items are two- and three-step problems, and this app
  answers one call.**
- **The lexicon gap is real but smaller than the table looks.** 7 of the 19 rows
  express every slot their call needs; 12 miss something. Five need chemistry the
  contract expects as an argument — a molar mass from a named compound, ions per
  formula unit, a mole ratio from a balanced equation. Two need a table that is
  not in the row. The rest is notation — state symbols, the degree sign, `1.0 M`
  — that the shipped 640-id vocabulary already spends 2 to 6 tokens on, so **no
  tokenizer change is needed for any of it**.

What would move the number, in the order the evidence supports: composition
reasoning (a compound table and formula parser, which unblocks 5 of the 19
blocking rows), multi-step composition (catalogue entry A3, where 23 of the 70
gated rows sit), and capacity. More words do not: every wording lever was closed
by measurement in [`closed-lines.md`](closed-lines.md). More rows from a
different source is a sourcing decision rather than an engineering one, and the
sourcing study has the number behind it — College Physics is 28% single-step
against this bank's 2.6%.

## The relation set

171 relations, in ten subject tags:

| Subject | Count | Examples |
|---|---|---|
| Physics | 56 | kinetic energy, momentum, projectile range, springs, waves and sound, Snell's law, lenses, fluid pressure, Bernoulli |
| Chemistry | 48 | molarity, pH, gas laws, stoichiometry, equilibrium, kinetics, thermochemistry, VSEPR bond angles |
| Units | 20 | km/m, Celsius/Fahrenheit, significant figures, percent change, ratios, rates, areas, volumes, probability |
| Modern physics | 9 | photon energy, photoelectric effect, de Broglie wavelength, Bohr levels, mass-energy, activity |
| Trig | 9 | sin/cos/tan, law of cosines, arc length |
| Algebra | 7 | sequences, factoring, completing the square |
| Logs | 7 | log base b, ln, half-life, compound interest |
| Optics | 6 | Snell's law, critical angle, lens and mirror equations, magnification, index from speed |
| Electricity and magnetism | 6 | Coulomb force, field strength, capacitors, parallel resistance, motional EMF |
| Thermodynamics | 6 | work at constant pressure, entropy, conduction, radiation, heat pumps |
| Precalc | 7 | quadratic roots, 2x2 system, `b^x = k`, average rate of change, vector arithmetic |
| Calculus | 2 | derivative and integral of a cubic |

The two physics rows above split `physics` rather than adding to it: all 171
relations are filed under the ten tags in `runtime/reltable.h`, and the
catalogue total is the sum of those tags, not of this table.

Adding relations is not free: each one renumbers the vocabulary and forces a
retrain, which is why a catalogue change is never a source-only pull. What the
retrain costs in accuracy is now measured rather than assumed: 84, 90 and 102
relations read 95.0%, 94.2% and 90.5% on the generated gate, 136 relations
read 90.3%, 153 relations read 90.2% on 500 held-out rows, and 171 relations
read 91.2% and 90.2% on the 2,000-row gate. Each of those figures is a
measurement at the catalogue size and on the set it names, not a statement about
the 171 relations the catalogue holds today. Two seeds of identical data differ
by up to 15 points on that 500-row probe at the 512-entry vocabulary and by 3.4
points at 640, so a single run is a sample rather than a property. Two seeds per
arm is the rule; `docs/tier1-plan.md` records the spread and what changed it, and
`docs/waves.md` holds the protocol.

## The tiers, and where each stands

`ROADMAP.md` holds the full reasoning. This is the scoreboard.

| Tier | What it is | State |
|---|---|---|
| 1 | Breadth: relations, model unchanged | **complete**, and well past it at 171 |
| 2 | Graphs: plot `y = f(x)`, pan, zoom, trace | **complete** (PR #8), and wave 4.6 added the plot of a relation's own call with the answer marked on it, parameter families, slope fields and subject diagrams. The function is typed on the screen's own input line and the plot carries the same title bar and key hints as the rest of the program, so no path enters through a message box |
| 3 | Expression engine: parse, differentiate, integrate | **done for what the roadmap asked**: Q16 parser, symbolic differentiation and integration, numeric integration, Taylor series, exact rational linear algebra, and differential equations — RK4, constant-coefficient closed forms, and separable forms including a rate that depends on x. What is not there is convergence tests, first-order linear equations with a non-zero x-dependent forcing term, and a computer algebra system, which is the next tier |
| 4 | Full CAS | not started, and deliberately last |

Tier 3 is the increment that pays without a retrain, because none of it touches
the model: the parser and evaluator answer expressions nobody anticipated, and
the linear-algebra path answers systems, determinants and inverses exactly.
Tier 4, a full computer algebra system, is what `ROADMAP.md` calls "a different
project that happens to share a screen". Wave 4 took the parts of it that a
student actually asks for — an antiderivative, a series, a differential
equation — and stopped at the boundary where honest refusal beats a guess.
`docs/wave4-plan.md` records where each piece stopped and why.

## Known gaps

Ordered by how much they matter, not by how easy they are.

1. **Never run on hardware.** The app cross-compiles, links for `arm926ej-s`,
   and packages into a `.tns` — the recipe is in `development.md` — but no one
   has run it on a CX II. Timing, memory pressure, and the actual UI are
   unmeasured. The screens are asserted by host tests over a 320x240 framebuffer
   — the 4.5 screen, the 4.6 plots, and the 2026-11-18 interface additions (the
   finder, the formula above the slots, the unit hints, the four refusal
   branches) — which proves the values and the layout arithmetic, not that they
   read well on glass. This is the largest unknown in the project. It also needs
   Ndless installed and matching the OS — see "Why the calculator refuses the
   file" below.
2. **The ceiling on real exam questions is the bank's shape, not our wording.**
   One correct call in 19 readable rows, a hard cap of 28 trustworthy single-call
   rows, and 23 of the 70 answerable-looking rows needing a second computation —
   see "The honest limit on real exam questions" above. The capability that
   moves it is a multi-step tape (A3 in `docs/capabilities.md`), not another
   wording round.
3. **The model is short of capacity, and every cheap lever is spent.** Corpus
   composition bought +0.45 points against a 2.6-point seed spread; contrast
   rows were flat on 333 held-out rows of the same eight pairs; register breadth
   moved the drift cohort from 41.6% to 47.6% on held-out structures while
   accuracy stayed flat; slot-cue selection lost 77 rows (74.0% → 70.5%). On
   long questions the failure is a choice among the numbers that are present,
   not an invented one: wrong argument values that appear somewhere in the
   question rise from 55% at 1–16 tokens to 79% at 25–32. The two levers left
   are capacity, which costs answer time on a 132 MHz ARM9, and argument
   verification in the runtime, which is code rather than data. Neither is
   decided, and neither should be chosen before the device's answer time is
   measured rather than estimated.
4. **Hand-written accuracy is gated per course, not in aggregate.** `gate.sh`
   gates generated phrasing at 80% and wrong relation at 5% over `evaluate.py
   --limit 1000` on the held-out generated set, and then runs three corpus
   gates: `generated structures still hard` over the drift cohort, a
   per-relation floor (`--max-thin 12`), and a per-course floor that scores each
   course's own calibrated rows and fails a build when one regresses. What is
   still not gated is the aggregate hand-written number — a loss spread thinly
   enough to stay inside every course floor's slack would pass — and the AP
   Chemistry slice, which no gate reads. That is why every comparison in this
   project is quoted with the set it came from.
5. **The window costs 11% of the real bank's single-call rows.** 3 of the 28
   exceed 192 tokens, and 9 of the 19 readable rows in the frozen slice run 197
   to 354 tokens. Raising the window is a retrain at `--seq 384` with attention
   cost ×4 per layer; it buys coverage of those rows and nothing on accuracy.
6. **`sig_figs` is the weakest relation** by exact calls. Every failure is a
   digit-copying error on a 4-digit number (`4188` becomes `418`). The guard
   rejects all of them, so the user sees "uncertain" rather than a wrong
   answer, but coverage is lost.
7. **`proj_range` is the largest single confusion** among wrong-relation cases
   (25 of 66 in a 2,000-problem diagnostic on the 66-relation set), but
   confusions are diffuse rather than concentrated: on the 287 hand-written rows
   at 25+ tokens there are 57 wrong-relation errors and the largest single pair
   occurs twice. No targeted fixture will fix them.
8. **Refusal is rare, so the model does not bail out on a hard question.** One
   row in 2,150 refuses across both seeds; long rows fail by answering wrongly.
   Any future argument-level rule has to beat that ratio, not merely improve
   its own predictor.
9. **The bank is the only independent test, and it is mostly unanswerable.**
   Every other number in this project is measured on phrasings written by the
   same process that wrote the solvers. That is worth stating plainly whenever a
   number is quoted as evidence about real students.

## How to verify the recorded state

```sh
sh stem-model/gate.sh
```

It prints PASS or FAIL per gate and exits non-zero unless all pass. If a
section is marked done in `PROGRESS.md` but its gate fails, the record is
wrong: set it back to `pending` and fix it before continuing.

The course floors score the 2,150 rows they were calibrated on, by default:

```sh
cd stem-model && python3 report_relations.py --course-floor
```

Scoring any other set against them is refused without `--allow-drift`, because a
floor whose denominator moves is not a floor. The same rule applies to the
training mix: `make_dataset.py`'s `injected_rows()` fills the hand-written share
exactly rather than rounding it down, so a corpus that grows cannot quietly
inject fewer real questions.

## How to build and deliver the calculator app

The app and the weights are two files that must be copied together:

| File | Size | What it is |
|---|---|---|
| `stemhelp.tns` | ~200 KB | the app |
| `weights.bin.tns` | ~5.9 MB | the int8 model, renamed so TI's transfer software accepts it |

```sh
cd stem-model/device && make
```

This needs the Ndless toolchain (`nspire-gcc`, `genzehn`, `make-prg`) on
`PATH`, and it packages whatever `stem-model/weights.bin` contains. That
weights file is gitignored, so a fresh clone must retrain and export before it
can build:

```sh
cd stem-model
make data && make train
uv run --with torch --with numpy --with sympy python export_model.py \
    --ckpt model.pt --out weights.bin
```

**The pairing is the trap.** A stale `weights.bin` with a fresh `bpe.h`
compiles cleanly and produces garbage calls, which looks exactly like a broken
model. The `tokenizer` target in `device/Makefile` regenerates `bpe.h` on every
build for this reason. Check `check_tokenizer_sync.py` if calls look wrong. A
regenerated header discarded during a revert leaves the built binary on the old
vocabulary, which is how one floor run read 2.1%: restore the header and rebuild
before measuring anything.

## Why the calculator refuses the file

`stemhelp.tns` is an Ndless program, not a TI document. On a calculator without
a working Ndless installation the OS cannot recognise it, and the message is
"This document format is not supported" — the same message it gives for any
Ndless app, and not a sign that the build is corrupt.

Two things must be true before the app will open:

1. **Ndless is installed and matches the OS.** This repository is a fork of
   Ndless r2023, which supports OS 6.2.0.333 and 6.4.0.74 on the CX II, CX II-T
   and CX II CAS. The install is the upstream one, unchanged: transfer
   `ndless_installer_4.5.5-6.2.0-6.4.0.tns` and `ndless_resources.tns` into a
   top-level folder named `ndless`, open the installer, and press an
   alphanumeric key. Check the OS version under Settings → Status before
   anything else; a newer OS rejects both the installer and every app built
   here.
2. **`ndless_resources.tns` runs.** Opening it on a working installation offers
   to uninstall Ndless. If it shows the same "format is not supported" message,
   Ndless is not installed correctly, and no `stemhelp.tns` will open until it
   is.

`weights.bin.tns` will always produce that message if opened, because it is the
model's raw bytes renamed to a `.tns` extension so TI's transfer software will
accept it. Place it in the same folder as `stemhelp.tns` and leave it alone; the
app loads it from disk on startup. If the weights were built from a different
tokenizer, the app now says so and names both vocabulary sizes rather than
blaming the question.

## Conventions that are easy to get wrong

- **Changing the registry invalidates three artifacts.** Adding or removing a
  relation requires regenerating the dataset, the tokenizer, and the
  checkpoint, in that order. Accuracy numbers from before the change do not
  apply to it.
- **Never hand-write expected solver values.** Generate the C test cases from
  the Python reference. `dec()` rounding and rational formatting are the usual
  source of mismatch.
- **The split is a hash, not a position.** `evaluate_paraphrase.py` holds out the
  rows whose `sha256("relation|question")` sorts highest, so no regeneration can
  shuffle a row between halves. The older "every fifth phrasing" rule described
  `surface.py`'s index list and no longer describes the paraphrase corpus.
- **The accuracy gate reads the percentage column**, not the count column. It
  evaluates 1,000 held-out problems; smaller samples are noisy enough to flip
  the verdict.
- **A measurement is not a number until it names its environment.** The weights
  file, the tokenizer and the corpus it was scored on are part of every figure;
  a figure without them cannot be reproduced, and one on this page could not be.
