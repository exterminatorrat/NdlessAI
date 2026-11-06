# Status

Where the project stands, what was planned, and what is left. Read this first
when resuming work; it is the map, and `stem-model/PROGRESS.md` is the
per-relation ledger.

Last updated: 2026-11-06, at commit `a8424b9`.

## One paragraph

The system reads an English word problem, a 5.2M-parameter model picks one of
66 exact solvers, and C code computes the answer. Tier 1 (breadth) is
complete: 66 relations across eight subjects, every gate green, and the model
retrained and measured at 91.6% exact calls and 2.9% wrong relation over 1,000
held-out problems. The device app builds to `stemhelp.tns`. The one thing
never done is running it on a physical calculator.

## What works today

| Capability | State | Evidence |
|---|---|---|
| 66 relations, Python reference | done | `sh stem-model/gate.sh` |
| 66 relations, C solvers | done | 13,335 assertions, 0 mismatches |
| Tokenizer, Python and C | done | 542/542 fixtures, sha `f2fdbea1be78763c` |
| Model picks the right tool | done | 91.6% exact, 2.9% wrong relation (1,000 held-out) |
| Grounding guard | done | refuses calls whose arguments are not in the question |
| Function graphing | done | `test_graph`, merged in PR #8 |
| Device build | done | `stemhelp.tns`, 201,632 bytes, valid Zehn header |
| On-calculator run | **not done** | no hardware in the build environment |

## The relation set

66 relations, by subject:

| Subject | Count | Examples |
|---|---|---|
| Physics | 15 | kinetic energy, momentum, projectile range, Ohm's law |
| Chemistry | 13 | molarity, pH, gas laws, stoichiometry |
| Units | 10 | km/m, Celsius/Fahrenheit, significant figures, mean |
| Trig | 9 | sin/cos/tan, law of cosines, arc length |
| Algebra | 7 | sequences, factoring, completing the square |
| Logs | 7 | log base b, ln, half-life, compound interest |
| Precalc | 3 | quadratic roots, 2x2 system, `b^x = k` |
| Calculus | 2 | derivative and integral of a cubic |

Nothing from the Tier 1 catalogue is deferred. `right_triangle`, `sinusoid`,
`cubic_solve` and `sig_figs` were each blocked on a missing helper (`atan`, a
multi-value output, rational-root deflation, an n-significant-figure
formatter); all four helpers now exist.

## The tiers, and where each stands

`ROADMAP.md` holds the full reasoning. This is the scoreboard.

| Tier | What it is | State |
|---|---|---|
| 1 | Breadth: 66 relations, model unchanged | **complete** |
| 2 | Graphs: plot `y = f(x)`, pan, zoom, trace | **complete** (PR #8) |
| 3 | Expression engine: parse, differentiate, integrate | not started |
| 4 | Full CAS | not started, and deliberately last |

Tier 3 is the next real increment. It is the jump from "pick a formula" to
"understand an expression": a tokeniser and parser for infix expressions with
precedence, symbolic differentiation, numeric evaluation with a root finder,
and numeric integration. It needs no model change, which is why it is
attractive.

## Known gaps

Ordered by how much they matter, not by how easy they are.

1. **Never run on hardware.** The app cross-compiles, links for `arm926ej-s`,
   and has a valid Zehn header, but no one has run it on a CX II. Timing,
   memory pressure, and the actual UI are unmeasured. This is the largest
   unknown in the project.
2. **`sig_figs` is the weakest relation**, at 80% exact calls against 91.6%
   overall. Every failure is a digit-copying error on a 4-digit number
   (`4188` becomes `418`). The guard rejects all of them, so the user sees
   "uncertain" rather than a wrong answer, but coverage is lost.
3. **`proj_range` is the largest single error class** among wrong-relation
   cases (25 of 66 in a 2,000-problem diagnostic). It passes the gate but is
   the most confusable relation.
4. **Digit copying generally.** The dominant failure mode is copying a
   plausible number from a distractor rather than choosing the wrong relation.
   The guard trades coverage for correctness by design.
5. **Closed phrasing set.** The phrasing banks are broad but finite.
   Performance on wording outside them is the main accuracy unknown.
6. **The retrain stopped at step 5,000 of 6,000.** The step-5,000 checkpoint
   (validation loss 0.0008) is what was exported and gated. The learning rate
   was already at 4.9e-05 of a 3e-04 peak, so the schedule was effectively
   complete, but the final 1,000 steps were not run.

## How to verify the recorded state

```sh
sh stem-model/gate.sh
```

It prints PASS or FAIL per gate and exits non-zero unless all pass. If a
section is marked done in `PROGRESS.md` but its gate fails, the record is
wrong: set it back to `pending` and fix it before continuing.

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
build for this reason. Check `check_tokenizer_sync.py` if calls look wrong.

## Conventions that are easy to get wrong

- **Changing the registry invalidates three artifacts.** Adding or removing a
  relation requires regenerating the dataset, the tokenizer, and the
  checkpoint, in that order. Accuracy numbers from before the change do not
  apply to it.
- **Never hand-write expected solver values.** Generate the C test cases from
  the Python reference. `dec()` rounding and rational formatting are the usual
  source of mismatch.
- **Every fifth phrasing is held out**, spread through the list rather than
  taken from the end, so the held-out set is not systematically the strangest
  wording.
- **The accuracy gate reads the percentage column**, not the count column. It
  evaluates 1,000 held-out problems; smaller samples are noisy enough to flip
  the verdict.
