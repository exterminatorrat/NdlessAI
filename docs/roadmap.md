# Roadmap: what this is, where it stands, and where a contributor starts

This page is the front door and an index. It repeats no number that another
page owns: [`status.md`](status.md) holds the state and the measurements,
[`waves.md`](waves.md) the plan of record, and [`capabilities.md`](capabilities.md)
every candidate the project could take on, including the ones recorded as not
worth doing. Start here, then read the page a section links.

## What it is

NdlessAI is an on-device STEM solver for the TI-Nspire CX II, built as a fork of
[Ndless](https://github.com/ndless-nspire/Ndless). A small int8 transformer reads
one typed word problem and picks one exact solver out of a fixed catalogue of 178
relations, together with that relation's numbers as arguments; the calculator
then computes the answer itself in integer and rational arithmetic. The model
never writes the answer, there is no network, and no floating point runs in the
hot path.

The property the design will not trade away is the honesty guard. An argument
must appear in the question as written, and a relation the decode barely chose
is refused rather than guessed at, so what a student sees is either an answer or
a refusal that names what went wrong — not a plausible wrong number. That
boundary between the model and the solvers is what
[`architecture.md`](architecture.md) describes, and
[`development.md`](development.md) is the working guide to it.

## Where it stands

Everything but hardware is built and measured: the model, the data pipeline, the
C engine and the `.tns` build are on master, and two seeds of the shipped pair
score 77.5% exact on the project's own 2,150 hand-written rows and 93.4% on 2,000
generated held-out rows, both at the 384-token reading window. Every course floor
passes on the rows it was calibrated on. On a real College Board AP Chemistry
bank it names a correct call for 2 of the 28 rows that name a catalogue relation
at all, and that gap is the bank's multi-step shape rather than its wording.

**The one thing that has never happened is a run on a physical calculator.**
Every screen claim rests on a host test over a 320x240 framebuffer, which proves
the arithmetic and the layout numbers, not that any of it reads well on glass;
answer time and memory pressure on the device are unmeasured. That is the largest
unknown in the project. [`status.md`](status.md) carries the full state and the
known gaps in the order they matter, and
[`ap-chemistry-ceiling.md`](ap-chemistry-ceiling.md) counts what closing the exam
gap would cost.

## What is being worked on

The ladder, in the order the evidence supports. Each step is costed in
[`ap-chemistry-ceiling.md`](ap-chemistry-ceiling.md), and none of them is a
promise that the number goes up.

1. **Teach the model to write a chain.** The runtime already executes and
   displays up to five calls; what is missing is a decoder that emits one. One
   training run with the chain share of the corpus raised, two seeds.
2. **Close the literal and notation shapes** that block the rest of the two-step
   set — a ratio written as two coefficients, an ion count inside a formula,
   exponent notation, comparisons the guard applies too strictly.
3. **Decide whether three- and four-step chains justify a wider window.** A
   decision rather than a task: the cost is answer time on a 132 MHz ARM9.
4. **Run it on a calculator**, which is a prerequisite for believing any of the
   rest rather than a final step.
5. **Capacity**, and only after the device's answer time is measured, because a
   larger model is paid for in seconds on that device.

The two levers short of capacity that remain are argument verification in the
runtime and the composition tape; every cheaper one has been measured and closed.

## What is refused, and why

These were proposed, built far enough to measure, and closed by their own
numbers. [`closed-lines.md`](closed-lines.md) carries each measurement with the
set it was taken on and the ratio a replacement has to beat; read it before
reopening any of them.

- **Corpus composition** — restoring 2,655 hand-written training rows bought
  +0.45 points against a 2.6-point seed spread.
- **Minimal-pair contrast rows** — flat on the 333 held-out rows that cover the
  same eight pairs the rows were built from.
- **Register breadth** — the hard-structure cohort moved from 41.6% to 47.6%
  while every accuracy measurement stayed flat.
- **Slot-cue selection** — a good predictor (81.9% of argument vectors against
  66.1% for reading order) and a bad change: 74.0% → 70.5%, a net loss of 77 rows.
- **Constraining decoded arguments to numbers the question already writes** —
  exact calls identical, and none of 253 re-decoded refusals became exact.
- **Dimension-based argument assignment** — perfect precision and 1.3 points of
  reach, because most rows write no unit to read.

The bar is a ratio, not a lift. On the hand-written split the model is right
about five times as often as it is wrong, so a rule that rewrites arguments has
to fix more than five rows for every one it breaks. A line reopens only with a
new fact — a second seed that flips a sign, a different set, or a model large
enough to move that ratio. "It might work with more data" is not a new fact, and
[`capabilities.md`](capabilities.md) also records what is deliberately out of
scope at all: a general CAS, tutoring, on-device training, and a bigger model as
the answer to a capability problem.

## Where a newcomer can help

Four things that are real today, unblocked, and checkable against the tree.

- **Run it on a CX II.** [`status.md`](status.md) has the build, the two files
  that must be copied together, and why the OS refuses the app without a
  matching Ndless install. What is wanted is the answer time on a real question
  and what the screens actually look like. No other item here unblocks as much
  with as little.
- **Bring one stale page current.** `docs/model.md` still describes a
  66-relation, 512-token model with 5,246,208 parameters;
  `docs/architecture.md` says the relation ids end at 431 and the merges start
  at 432, where today they end at 438 and start at 439; and `docs/status.md`,
  `docs/capabilities.md` and `docs/expansion.md` link `ROADMAP.md`, which is
  `stem-model/ROADMAP.md`. One page per pull request, checked against
  `runtime/reltable.h`, `runtime/bpe.h` and the model the tree describes today.
- **Write hand-written rows for the 25 relations that have none.** Every
  relation has generated phrasings, but 153 of the 178 carry a hand-written row
  in `fixtures/`, so the other 25 have no hand-written measurement at all:
  `avg_rate`, `capacitor_charge`, `capacitor_energy`, `conduction_rate`,
  `coulomb_force`, `electric_field_point`, `entropy_change`,
  `excess_concentration`, `excess_moles`, `gas_heat_cp`, `gas_work_isobaric`,
  `heat_pump_cop`, `heat_reaction`, `ion_from_ksp`, `ions_from_mixture`,
  `kc_from_conc`, `kc_from_ice`, `mass_percent`, `min_of`, `mole_fraction`,
  `moles_from_molarity`, `motional_emf`, `parallel_resistance`,
  `radiation_power`, `stoich_mass`. The format, the hash-based split and the
  two properties a row must satisfy are in `fixtures/README.md`, and
  `python3 evaluate_paraphrase.py --check-corpus` validates a new file without
  weights or a binary. Say plainly in the pull request that this adds
  measurement coverage and not accuracy: corpus composition and register
  breadth were both closed by measurement, so no accuracy number should move.
- **Package the weights into the app.** `stemhelp.tns` loads `weights.bin.tns`
  from disk, and a weights file built from a different tokenizer loads without
  any complaint and answers every question wrongly. Entry E6 in
  [`capabilities.md`](capabilities.md) calls that the most expensive debugging
  trap in the repository and asks for the weights to be embedded or appended, so
  the two files cannot be mismatched. No retrain and no measurement needed, and
  a host test can assert the pairing rather than hoping for it.

## Licensing and the text rules

The rules a contributor can break without meaning to, so they come before the
process:

- **No third-party question text in the repository** — not in the corpus, not in
  a fixture, not as an example in a pull request. The sourced pools may be read
  to learn registers and sentence frames; their sentences are never copied and
  never closely paraphrased. [`wording-ledger.md`](wording-ledger.md) names each
  pool, its licence and which relations it informed.
- **No more than six consecutive words may match any source.** Every generated
  row is authored from a spec, not rewritten from a source sentence.
- **The College Board bank and school material stay local and gitignored.**
  Nothing that quotes a bank row enters a commit; pages that analyse the bank
  cite item ids, not text.
- **Permissively licensed sources** (CC BY-SA, CC0, public domain) may be used
  more freely, and are still attributed.
- **The repository is a derivative of Ndless and keeps its licence**: most of it
  under the Mozilla Public License 1.1, and components with their own
  `LICENSE.txt` or `LICENSE.html` under those terms. Read
  [`Mozilla-Public-License-v1.1.html`](../Mozilla-Public-License-v1.1.html)
  before redistributing any part of it.

## Decisions that are not a contributor's to take

- **The reading window, the model size and where corpus text comes from are the
  owner's calls.** Each has a measured trade-off attached, and a weekend spent
  on a window change or a wider model is not a contribution until the choice is
  made. The evidence on the window and capacity is in the known gaps of
  [`status.md`](status.md); the exam-side cost is steps 3 to 5 of
  [`ap-chemistry-ceiling.md`](ap-chemistry-ceiling.md).
- **There is no visible work queue.** The development tracker is private, and
  this repository has GitHub Issues and Discussions both switched off. Propose
  work in a draft pull request that changes this page or
  [`capabilities.md`](capabilities.md), or ask on an open pull request. Waiting
  for a tracker you cannot see is not a plan.

## Which document to read next

| Page | What it holds |
| --- | --- |
| [`status.md`](status.md) | Where the project stands, every measurement with its set, and the known gaps in order |
| [`waves.md`](waves.md) | The plan of record: wave order, workstreams, file ownership, and what done means |
| [`capabilities.md`](capabilities.md) | Every capability with its cost and retrain flag, plus the deliberately-not-doing list |
| [`closed-lines.md`](closed-lines.md) | The wording and selection lines that were measured and closed, and the ratio to beat |
| [`composition.md`](composition.md) | Which relations chain, how far the graph reaches, and what a chain buys on the exam bank |
| [`ap-chemistry-ceiling.md`](ap-chemistry-ceiling.md) | The real-exam ceiling, counted, and the ladder out of it with costs |
| [`tier1-plan.md`](tier1-plan.md) | The course floors: what each course is scored on and why the denominator is fixed |
| [`development.md`](development.md) | Host workflow, tests, cross-compilation, the `.tns` build, and adding a relation |
| [`architecture.md`](architecture.md) | Boundaries, the call contract, fixed-point formats, and failure behavior |
| [`expansion.md`](expansion.md) | The same candidates ranked by value per unit of work, when a decision is needed |
| [`model.md`](model.md) | Model notes, describing an earlier and smaller model — read `status.md` for today's |
