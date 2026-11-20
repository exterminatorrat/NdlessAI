# The AP Chemistry ceiling

What reaching 100% of the College Board AP Chemistry question bank would take,
and why it is not a reachable goal for this design. Every number here names the
set it was measured on; the last section names the command or report behind each
one, because two figures with the same name and different sets cannot be compared.

Measured against the shipped r9 pair — `weights_apchem_r9_s1.bin` and
`weights_apchem_r9_s2.bin`, 178 relations, 384-token window, master `57761dd4`.
Nothing on this page is a forecast.

## The goal worth aiming at

**Answers the numeric AP Chemistry questions it claims to answer, and refuses the
rest honestly.**

That is verifiable: it is a row count on a fixed slice, it can fail, and a
student can tell whether it held. A percentage of the bank is not, because most
of the bank is not a numeric question this architecture can represent at all —
73.5% of its multiple-choice rows name no relation in the catalogue, and a third
of it asks for an explanation rather than a number. Chasing a percentage of that
denominator would push the implementation toward guessing on questions nobody can
score, which is the one outcome the grounding guard exists to prevent.

## Why 100% is unreachable

Not model quality. The **shape** of the questions: what they ask for, and what
they expect a reader to already know.

### The bank, counted

1,136 multiple-choice rows, by what they name:

| Gate | Rows left |
| --- | --- |
| names exactly one catalogue relation | 250 (22%) |
| names two or more, so ambiguous | 51 |
| names no catalogue relation at all | 835 (73.5%) |
| one relation named and the argument count present | 122 |
| and at least three quantity-shaped options | 70 |
| and survives a hand read as one call | **28** |

The hand read labelled all 70: **28 `one_call`**, **23 `two_step`**, 9
`wrong_relation` (a cue word matched, the ask is a different quantity), 6
`needs_constant` (an argument is a tabulated constant absent from the text), 3
`no_relation`, 1 `broken_text`. The mechanical gate above them is right 40% of the
time, which is why the labels exist.

**The numeric, self-contained ceiling is 51 rows — 4.5% of 1,136.** 28 rows are
one call deep and 23 need a second computation; `two_step` means the relation the
row names is only an intermediate, not that exactly one more call finishes it. The
other 1,085 rows are not unattempted work. They are rows this design cannot
answer: no catalogue relation, an explanation, a figure, or a constant the
question expects the student to have memorised.

One caution about the denominator: 301 MCQ rows lost their options to PDF
extraction and are counted as unjudged, not as unanswerable. A limit of the file
must never be reported as a limit of the program.

### Three structural walls

1. **A third of the bank asks for an explanation.** "Compare", "justify",
   "predict": 33% of bank items close on a reason rather than a quantity, against
   17–18% in the textbooks the same ruler was run over. This architecture answers
   with a number or refuses. Extra relations do not reach these rows.
2. **38% of items depend on a figure or a table.** The exam register is the most
   figure-dependent of the four sources measured — the opposite of the textbook
   reputation. A drawn graph, a data table and a species diagram are all outside a
   text-only pipeline.
3. **Some rows carry no data at all.** On the 19 readable rows of the frozen AP
   slice, **two point at information that is not in the row** — a stem that refers
   to "the information above", or to a half-reaction printed elsewhere. No lexicon,
   relation or model size recovers a number that was never transmitted.

### Where it stands today

| Measure | Set | Result |
| --- | --- | --- |
| AP Chemistry slice, exact call | 28 pinned rows, r9 | **2 of 28** seed 1 (7.1%, 95% interval 2.0–22.6%), **1 of 28** seed 2 |
| Hand-written, exact | 2,150 calibrated rows, r9 two-seed mean | **77.5%** (77.7 / 77.4) |
| Generated held-out, exact | 2,000 rows, r9 two-seed mean | **93.4%** (93.8 / 93.1) |
| Two-step bank rows computable end to end | 23 hand-read rows, chain named by hand | **7** |
| Two-step bank rows the model writes a chain for | 7 end-to-end rows through `runtime/main_device` | **0** — every output was a single `CALL` |

The gap between the last two rows is the whole problem in one line. Seven of the
23 rows are arithmetic the runtime already executes, grounds and displays when a
chain is handed to it; on the seven rows that were run through the device path,
the model named a single relation every time, and on only 1 of 7 (seed 1) and 2 of
7 (seed 2) did it name the relation the chain would start with.

## The ladder

Five steps, in the order the evidence supports, each with what it costs and how
likely it is to work. None of them is a promise that the number goes up.

### 1. Teach the model to write a chain

One training run with the chain share of the corpus raised and nothing else
changed. The chain target already exists and is already in the shipped corpus —
600 two-call rows from `gen_chain_cases.py`, each exact-checked against `plan.c`'s
own arithmetic, all 600 inside the training window — and the decoder can carry a
chain once it has one. What the 600 rows did not do is teach the format: inside
250,000 training rows they are 0.24% of what the model sees, and the shipped pair
still emits a single call on every real row measured — fourteen outputs, seven
rows on each of two seeds, none of them a chain. The step is to raise that share,
not to add rows.

**Cost:** one training run and its measurement, about three hours on this machine
at the 384-token window (the r9 seeds took 2 h 40 m and 2 h 47 m of training alone,
plus ~25 minutes of measurement each). Two seeds if the result is to be trusted
against the 3.4-point spread this vocabulary carries — so budget twice that.

**Likelihood:** unknown, and this is the single largest jump available. It may
also fail, and it would fail in a way worth recording: a corpus that teaches chain
format while the decoder still cannot name a relation the checkpoint was not
trained on.

### 2. Close the shapes that stop the rest of the two-step set

Most of the 23 are already expressible. Ten chains are arithmetically exact and
refused on a *literal* rather than a relation: four carry a ratio the question
writes as two coefficients, two write an ion count inside a formula, two are
numbers the guard compares too strictly (`0.0300` against `0.03`), two are in
exponent notation. Two more wait only on `reltable.h` being regenerated with
`bpe.h`, which the retrain does.

Three rows have no chain of catalogue relations at all: one whose value lives in a
graph, one Kp with two product species, and one that needs six calls where
`PLAN_MAX_STEPS` is five. Those three may be genuinely unanswerable by this
design, and they should be recorded as such rather than modelled around.

**Cost:** code and fixtures, no training for the literal shapes; the relation
names and the retrain for the rest.

**Likelihood:** high for the literal and notation shapes — they are defects in
comparison and normalisation, not arithmetic. Low for the three structural rows.

### 3. Three- and four-step chains

Fourteen of the 23 rows need more than one call, and eight of those need three or
more (six three-call, one four-call, one five-call). The runtime executes up to
five steps already. What is missing is the reading window on the *training* side:
at 193 tokens only 153 of 189 three-call chain rows fit, and 27 of 73 four-call
rows.

**Cost:** a wider window and its arithmetic on a 132 MHz ARM9, which is the cost
this project has already paid once (384 tokens cost 3.9–5.1× the training wall
clock) and would pay again.

**Likelihood:** it is a decision, not a task. Depth is cheap in the catalogue — the
composition graph reaches 106 relations at four calls — and rare in the bank: a
handful of rows. Deciding whether four-call chains are worth a window is the
question, and the answer may reasonably be no.

### 4. Accuracy on what is already reachable

77% exact on the hand-written set the project wrote itself, and 2 of 28 on a
slice drawn from the real bank. The dominant remaining failure is not
comprehension: the model reads the question and takes the wrong number out of it,
and every grounded-selection rule measured so far made that worse rather than
better (the closed lines are in [`closed-lines.md`](closed-lines.md)).

**Cost:** a larger model. On this device that costs answer time, and the device
limit has never been measured on glass. Weeks of work, on a lever already named in
[`status.md`](status.md) as capacity.

**Likelihood:** unguaranteed. The two measured attempts to buy accuracy without a
wider model — corpus composition and argument selection — were flat and negative.

### 5. Run it on a physical calculator

Nothing in this project has ever executed on hardware. Every screen claim is a
host test over a 320×240 framebuffer, which proves the layout arithmetic and the
values, not that they read well on a 132 MHz device with a real keypad. This is a
prerequisite for calling any of the rest meaningful, not a final step.

**Cost:** one CX II with a matching Ndless install and an afternoon; the app is
already built (`stemhelp.tns`, 339,292 bytes, `arm926ej-s`).

**Likelihood:** high that it runs, unknown what it reveals. Timing, memory
pressure and readability are all unmeasured.

## What this page does not claim

- It does not claim the 51-row ceiling is stable. It is a hand read of 70
  mechanically gated rows, and a different reader could move a row or two between
  `one_call`, `two_step` and `wrong_relation`.
- It does not claim the AP Chemistry slice's 2 of 28 is the app's accuracy on AP
  Chemistry. It is a measurement of 28 rows the bank can supply, and the slice's
  own denominator changed when the window widened (9 of the 28 were unreadable at
  192 tokens and are measurable at 384).
- It does not claim any ladder step will work. Steps 1 and 4 may both fail, and
  the point of writing them down with their costs is that a failure is then a
  result rather than a surprise.

## Where each number came from

| Number | Measurement | Source |
| --- | --- | --- |
| 1,136 MCQ rows; 250 / 51 / 835 | `apchem_ingest.py --no-ocr` over the bank, then a cue match per item | NDLE-23; `stem-model/corpora/apchem/` (gitignored), audited by `apchem_audit.py` |
| 122 / 70 / 28 of 1,136 | argument count, then three-or-more quantity-shaped options, then a one-label-per-row hand read of all 70 | NDLE-23, table "Where the rows go" |
| 28 / 23 / 9 / 6 / 3 / 1 of 70 | the same hand read, one label per row | NDLE-23 |
| 33% explanation, 38% figure/table | a fixed cue list run over every item of four sources | NDLE-23, register study (no question text stored) |
| 2 of 19 rows pointing at absent data | the 19 readable rows of the frozen slice, scored against the tokenizer `bpe.h` encodes | NDLE-25, "The lexicon and notation gap" |
| 2/28 and 1/28, 95% interval 2.0–22.6% | the 630 frozen eval ids, 28 naming a catalogue relation, r9 seeds 1 and 2 through `runtime/main_device` at `--seq 384` | NDLE-33; interval is Wilson on 2 of 28 |
| 77.5% on 2,150; 93.4% on 2,000 | two seeds, exact call, `report_relations.py --course-floor` and the held-out gate | NDLE-33, "Measured, all at `--seq 384`" |
| 7 of 23 answer end to end | `compose.py --rows fixtures/chain_rows.txt`, then each chain run through the runtime with the row's own numbers | [`composition.md`](composition.md); rows listed there by id |
| 20 chains, 6 / 6 / 6 / 1 / 1, 3 blocked | the same `compose.py --rows` derivation, after the relation batch | [`composition.md`](composition.md), "The 23 second-computation rows" |
| 0 of 7 emitted a chain | seven end-to-end rows through `runtime/main_device`, both seeds, every output a single `CALL` | NDLE-33, "The seven end-to-end rows" |
| 153 of 189 three-call chains fit 193 tokens | `gen_chain_cases.py` rows measured with the shipped tokenizer | [`composition.md`](composition.md), the window table |
| ~3 h per training run at 384 | r9 seed 1 9,771 s and seed 2 9,077 s of training on this machine | NDLE-33, "The run" |

The AP Chemistry corpus itself stays local and gitignored, as the licensing rule
requires: nothing on this page quotes a bank row, and the ids above are references
into Harry's own material, not text.
