# Lines that were measured and closed

Four ideas about wording and argument selection were proposed, built far enough
to measure, and closed by their own numbers on 2026-11-18. They are recorded
here so nobody spends another run on them, and because a negative result is a
result: each one names the set it was measured on, and where there is a way back,
the fact that would open it.

Reopen one only with a new fact — a second seed that flips a sign, a different
set, or a model large enough to change the ratio below. "It might work with more
data" is not a new fact.

## The bar every replacement has to clear

On the 2,150-row calibrated hand-written eval split (`courses.FLOOR_CORPUS`,
`courses.FLOOR_EVAL_ROWS`), the shipped model is right about five times as often
as it is wrong: seed 1 of the r5 pair reads 1,591 exact calls (74.0%), 328 right
relation with wrong arguments (15.3%), 230 wrong relation (10.7%) and 1 refusal.
A rule that rewrites arguments therefore has to fix **more than five times** as
many rows as it breaks. That ratio, not a rule's accuracy and not its lift over a
baseline, is what predicts whether an argument-level idea pays.

## 1. Corpus composition — +0.45 points against a 2.6-point seed spread

**The claim.** The hand-written share of training was being under-filled, so
restoring it would buy accuracy.

**The measurement.** `make_dataset.py` divided the 50,000-row paraphrase budget
by the corpus size and dropped the remainder: 19 passes of 2,618 rows (49,742)
at r5, 17 passes of 2,785 (47,345) at r6. A corpus that grew injected fewer real
questions. `injected_rows()` now fills the share exactly (#152), so r7 carries
250,000 train rows of which exactly 50,000 are hand-written — 2,655 rows
restored over r6.

**The result.** On the 2,150-row calibrated hand-written split, two seeds each:
70.45% (r6 mean) → **70.9% (r7 mean), +0.45 points**, against a **2.6-point
spread between r7's own two seeds**. The generated gate over 2,000 held-out rows
read 89.1% → 89.8%. Both movements are inside the noise.

**Closed.** Corpus composition is not an accuracy lever at this corpus size.
Score every later corpus change against the seed spread before believing it.

## 2. Minimal-pair contrast rows — flat on held-out

**The claim.** Training on the pairs the model actually confuses would fix
wrong-relation errors.

**The measurement.** 84 candidate pairs were filtered against
`weights_apchem_s1.bin`'s own misses, leaving five; 41 contrast rows were
generated, each pair on one argument tuple so a confusion can only show up as
the wrong call (#140). All 41 rows are `split: train`, so the 100% they score is
memorisation and is not evidence.

**The result.** On the **333 held-out eval rows** covering the same eight pairs:
80.05% (r5 mean) → 81.1% (r6) → **80.95% (r7), −0.15 points against r6.** Flat.

**Why it was never likely.** The confusions are diffuse. On the 287 hand-written
rows at 25+ tokens there are 57 wrong-relation errors and the largest single
confusion pair occurs twice, so no small set of contrast rows localises them.

**Closed.** Contrast pairs teach nothing measurable on held-out rows at this
size.

## 3. Register breadth — the drift cohort moved, the accuracy did not

**The claim.** Widening the registers the corpus covers — ask position as a
structural axis, equipment and idealisation clauses, the declared semicolon
list, equilibrium-from-a-state, per-relation species and modifiers — would raise
accuracy on wording the model has not seen.

**The result.** The ledger grew from 2,970 structures (#130) to 13,530 (#133),
and the drift cohort — held-out structures written to be hard — went from
**41.6% exact on 1,838 held-out structures** (#131) to **47.6% at r6** and
47.35% at r7 **on 2,697 held-out structures**. Harder structures were learned,
which is what the register work asked for.

Nothing else moved: generated gate 90.7% (r5 mean) → 89.1% (r6) → 89.8% (r7) on
2,000 held-out rows; hand-written 72.5% → 70.45% → 70.9% on the 2,150-row split;
every course floor passing on its own calibrated set throughout.

**Closed.** Register breadth buys coverage of a register, not accuracy. The
corpus is broader and the student's number is where it was.

## 4. Slot-cue selection — 74.0% → 70.5%, a net loss of 77 rows

**The claim.** The words around a number name its slot, so a rule that reads
them can repair wrong arguments.

**The rule is a good predictor.** Scoring every number against every slot and
taking the best whole assignment gets **81.9% of whole argument vectors** on the
1,685 selectable rows of the 2,150-row split, against 66.1% for filling slots in
reading order; it is stable across halves, and it collapses to 53.2–57.6% when
each relation is given another relation's cue bank, so the words are doing the
work and not the machinery.

**And a bad change.** Against the shipped model's own per-row output, on the
1,919 rows where it names the right relation:

| bucket | rows | model exact | fixed | broken | net | rule exact |
| --- | --- | --- | --- | --- | --- | --- |
| 1–16 words | 942 | 88.3% | 68 | 112 | **−44** | 83.7% |
| 17–24 words | 748 | 79.8% | 76 | 117 | **−41** | 74.3% |
| 25–32 words | 191 | 73.3% | 29 | 21 | **+8** | 77.5% |
| 33+ words | 38 | 60.5% | 8 | 8 | **0** | 60.5% |
| all | 1,919 | 83.0% | 181 | 258 | **−77** | 78.9% |

End to end on the whole set that is **74.0% → 70.5%**. The rule fixes 55% of the
argument errors (181 of 328) and breaks 16% of the already-correct calls (258 of
1,591); with five correct calls per error it needs to fix five times what it
breaks, and it does not. Sweeping margin 0.3–2.0 against separation 0.6–0.9,
**no operating point is positive overall** — the best cell in the grid is −77.

**The one surviving cell is not a result.** At 25–32 words the rule gains 8 rows
of 234 (59.8% → 63.2%), which is 3.4 points on one seed — the seed spread at this
vocabulary, not a movement. Reopen the line only if the same predictions for
`weights_apchem_s2.bin` show that bucket positive again; otherwise it is closed.

## Two earlier lines, same bar

- **Constraining decoded arguments to numbers the question writes** was built as
  a single retry and reverted: exact calls identical to the row (1,577 and 1,560
  on the 2,150-row split), refusals cut from 164/160 to 34/37, and **none of the
  253 re-decoded refusals became exact** — 124 came back as the right relation
  with the wrong arguments, 87 errored, 42 picked the wrong relation. A mask
  reaches which numbers are available; the model's error is a choice among the
  numbers that are already there.
- **Dimension-based argument assignment** decides 56 of 139 permuted rows, all
  56 correctly, for **1.30 points** of hand-written exactness, and 1.37 with
  quantity nouns. Reach, not precision, is the wall: 83 of the 139 rows carry no
  dimension signal to read.
