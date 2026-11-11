# Status

Where the project stands, what was planned, and what is left. Read this first
when resuming work; it is the map, and `stem-model/PROGRESS.md` is the
per-relation ledger.

Last updated: 2026-11-11, at commit `2beba30`.

## One paragraph

The system reads an English word problem, a 5.2M-parameter model picks one of
102 exact solvers, and C code computes the answer. Waves 1 through 3 are on
master: 136 relations across thirteen subject modules, the typed expression path for
calculus and linear algebra, and the wording work that took hand-written
accuracy from 43.4% to 72.3%. The generated-phrasing gate is 90.5%, down from
95.0% at 84 relations, because the catalogue outgrew the model rather than the
training budget — `docs/waves.md` carries that measurement. The device app
builds to `stemhelp.tns`. The one thing never done is running it on a physical
calculator, and it takes Ndless on a supported OS to get that far.

## What works today

| Capability | State | Evidence |
|---|---|---|
| 136 relations, Python reference | done | `sh stem-model/gate.sh` |
| 136 relations, C solvers | done | eleven per-subject units; `test_solvers` reports its assertion count on every run |
| Tokenizer, Python and C | done | `test_bpe` round-trip, `check_tokenizer_sync.py` against `data/tokenizer.json` |
| Model picks the right tool | done, and the current limit | 71.8% exact on 1,942 hand-written rows; 90.3% exact and 1.7% wrong relation on the generated gate; 60/60 through the full on-device path |
| Grounding guard | done | refuses calls whose arguments are not in the question |
| Typed expressions, no model needed | done | `integrate(x^2,0,3)` answers 9 and `d/dx`, `det`, `inv`, `rank`, `solve` answer on exact rationals; `integrate(x^2)` answers `x^3/3 + C`, `series(exp(x),0,5)` a Taylor polynomial, `ode(2y,0,1,1,64)` a numerical solution and `desolve(2y,0,1)` its closed form `exp(2x)` |
| Function graphing | done | `test_graph`, merged in PR #8 |
| Device build | done | `stemhelp.tns` and `weights.bin.tns`, cross-compiled for `arm926ej-s` |
| On-calculator run | **not done** | no hardware in the build environment, and it needs Ndless on a supported OS |

## The relation set

136 relations, in thirteen subject modules:

| Subject | Count | Examples |
|---|---|---|
| Physics | 54 | kinetic energy, momentum, projectile range, springs, waves and sound, Snell's law, lenses, fluid pressure, Bernoulli |
| Chemistry | 29 | molarity, pH, gas laws, stoichiometry, equilibrium, kinetics, thermochemistry |
| Units | 10 | km/m, Celsius/Fahrenheit, significant figures, mean |
| Modern physics | 9 | photon energy, photoelectric effect, de Broglie wavelength, Bohr levels, mass-energy, activity |
| Trig | 9 | sin/cos/tan, law of cosines, arc length |
| Algebra | 7 | sequences, factoring, completing the square |
| Logs | 7 | log base b, ln, half-life, compound interest |
| Optics | 6 | Snell's law, critical angle, lens and mirror equations, magnification, index from speed |
| Electricity and magnetism | 6 | Coulomb force, field strength, capacitors, parallel resistance, motional EMF |
| Thermodynamics | 6 | work at constant pressure, entropy, conduction, radiation, heat pumps |
| Precalc | 3 | quadratic roots, 2x2 system, `b^x = k` |
| Calculus | 2 | derivative and integral of a cubic |

Adding relations is not free: each one renumbers the vocabulary and forces a
retrain, which is why a catalogue change is never a source-only pull. What the
retrain costs in accuracy is now measured rather than assumed: 84, 90 and 102
relations read 95.0%, 94.2% and 90.5% on the generated gate, and 136 relations
reads 90.3%. `docs/waves.md` holds the table and the protocol behind it.

## The tiers, and where each stands

`ROADMAP.md` holds the full reasoning. This is the scoreboard.

| Tier | What it is | State |
|---|---|---|
| 1 | Breadth: relations, model unchanged | **complete**, and well past it at 102 |
| 2 | Graphs: plot `y = f(x)`, pan, zoom, trace | **complete** (PR #8) |
| 3 | Expression engine: parse, differentiate, integrate | **done for what the roadmap asked**: Q16 parser, symbolic differentiation and integration, numeric integration, Taylor series, exact rational linear algebra, and constant-coefficient differential equations. What is not there is convergence tests, separable equations with x-dependent coefficients, and a computer algebra system, which is the next tier |
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
   and has a valid Zehn header, but no one has run it on a CX II. Timing,
   memory pressure, and the actual UI are unmeasured. This is the largest
   unknown in the project. It also needs Ndless installed and matching the OS —
   see "Why the calculator refuses the file" below.
2. **The model is out of capacity, not out of data or steps.** Hand-written
   accuracy sits at 72.3% and moved inside the noise across 84, 90 and 102
   relations (72.6%, 73.8%, 72.3%) while the generated gate fell from 95.0% to
   90.5%. Four rounds of wording data flattened at round three; round four is
   parked on a branch. The next lever is a wider model, which costs answer time
   on a 132 MHz ARM9, or argument verification in the runtime, which is code
   rather than data. Neither is decided.
3. **Hand-written accuracy is not a gate.** `gate.sh` gates generated phrasing
   at 80% and wrong relation at 5%, both on the generated set. Nothing fails a
   build when the number a student experiences gets worse, which is why every
   wording comparison in this project is quoted with the set it came from.
4. **`sig_figs` is the weakest relation** by exact calls. Every failure is a
   digit-copying error on a 4-digit number (`4188` becomes `418`). The guard
   rejects all of them, so the user sees "uncertain" rather than a wrong
   answer, but coverage is lost.
5. **`proj_range` is the largest single error class** among wrong-relation
   cases (25 of 66 in a 2,000-problem diagnostic on the 66-relation set). It
   passes the gate but is the most confusable relation.
6. **Digit copying generally.** The dominant failure mode is copying a
   plausible number from a distractor rather than choosing the wrong relation.
   The guard trades coverage for correctness by design.
7. **Closed phrasing set.** The phrasing banks are broad but finite.
   Performance on wording outside them is the main accuracy unknown.

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
app loads it from disk on startup.

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
