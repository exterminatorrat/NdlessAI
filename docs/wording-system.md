# The wording system

A spec-driven generator for question wording: every relation declares the roles
its slots play, a sentence grammar composes those roles into clauses, and a
validator refuses any row whose text does not decide its own call. Chemistry is
the first subject built end to end, because it has the most relations and the
only external reality check in the repository.

This document is the shape to argue with, before the code that implements it.

## The problem, measured

The model reads a sentence and emits a call. The sentence is therefore the whole
interface, and today there are about ten of them per relation:

| bank | rows | accuracy |
| --- | --- | --- |
| generated gate, 2,000 held-out rows | 2,000 | 90.7% (two-seed mean) |
| hand-written paraphrase split, eval | 2,150 | 72.5% |
| real College Board questions the bank can answer | 28 | 10.0% (seed 1), 0.0% (seed 2) |

The gap between the first two rows is the measure of how much of the generated
gate is memorised structure rather than reading. `surface/` holds 48 chemistry
relations with 493 sentences between them (8 to 13 each), all in the imperative
register, all with a fixed number format, and `surface/vary.py` multiplies them
lexically — notation, money, spelling, opener. Lexical variation over ten
structures is still ten structures: the model sees `Find the molarity.` a hundred
thousand times with different numbers.

Two concrete failures the current bank cannot express:

**`percent_change` against `percent_error`.** Same two slots, same arithmetic
shape, and the sentences already carry the deciding word — "percent change"
against "percent error". The model confuses them at 4 rows, because nothing in
training ever sets the two sentences side by side with the same numbers and one
word changed, which is the only arrangement that teaches a discriminator rather
than a topic.

**`carnot` against `carnot_efficiency`.** These two produce the *same quantity in
different formats* under *the same sentence*: "A Carnot engine operates between
300 K and 500 K. Find its efficiency." returns `50%` from one relation and `0.5`
from the other, and the templates differ only in argument order. No amount of
wording can decide that pair, because the wording does not contain the choice.
This is a spec defect surfacing through wording, and the system's job is to
report it in a ledger rather than to generate three hundred sentences that
pretend to resolve it.

## What already exists, and what it cannot do

- `surface/*.py` — hand-written `TEMPLATES` per subject. Sentences, not
  structures; variation is lexical, number formats are whatever each template
  happened to write.
- `surface/vary.py` — four rewrite families with two choices held out for a
  transfer cohort. Lexical, so it multiplies rows without multiplying structure.
- `distractors.py` — one extra sentence per relation, unrelated to roles.
- `relations.py` — `params`, a generator, a solver, an optional step template.
  Nothing declares what a slot *is*: its dimension, its unit, or whether it is
  interchangeable with another slot.

The last point is the one that matters. NDLE-21 measured it: of 139 rows holding
the right numbers in the wrong slots, a dimension-based assigner decides 56, and
83 are undecidable because the arguments carry no units, or two slots hold the
same kind of quantity, or the units written contradict the slot. **Dimension is
not a property the sentence can be trusted to carry.** It has to be a property
the generator refuses to omit.

## The shape

Five parts, each independently testable. Chemistry first.

### 1. `surface/spec.py` — the relation spec

Per relation, declare the roles, not just the parameter names:

    SPEC["molarity"] = RelationSpec(
        slots={"n": Slot("amount of solute", "amount", "mol"),
               "V": Slot("volume of solution", "volume", "L")},
        answer=Slot("molarity", "concentration", "mol/L"),
        names=("molarity", "molar concentration", "concentration in mol/L"),
        confusable=("mass_from_molarity", "dilution", "mixing_concentration"),
        register=("exam", "lab", "classroom"),
    )

`dim` is symbolic, so interchangeability is computable: two slots with the same
`dim` are the ambiguous pairs, and the composer must name both of them or refuse
the sentence. That is the design decision worth arguing: NDLE-21 showed that
*inferring* the assignment from dimensions fails on the majority of cases, so
this system does not infer it — it makes naming a slot a structural requirement,
and the validator enforces it. An ambiguous sentence becomes unrepresentable
rather than merely unwise.

`confusable` is the league table of near-neighbours, taken from the measured
confusion pairs. It drives the contrast frames: for each unordered pair, one
frame per relation with the same numbers in the same roles and only the deciding
tokens differing.

### 2. `surface/grammar.py` — clause library and composer

Not a sentence list. Three clause families per subject, composed:

    GIVENS   "A sample contains {m} g of {species}."
             "You dissolve {n} mol of solute in water and bring the volume to {V} L."
    FRAMING  "Assume the volumes are additive."   "The vessel is held at constant temperature."
    ASK      "What is the {answer}?"   "Report the {answer} in {unit}."
             "Determine the {answer} to three significant figures."

A structure is a triple of (given clause, framing clause, ask clause) plus the
set of roles each clause names. Every slot must be consumed exactly once; every
same-dimension pair must be named; the ask must name the answer quantity. Those
three rules are checked before a row is emitted, which is what makes a generated
row valid by construction rather than by sampling luck.

Chemistry clause families to write first, because the bank's failures are here:
statement of givens (mass, moles, volume, pressure, temperature, concentration),
the reaction framing (balanced equation, limiting reagent, complete dissociation,
equilibrium), and the ask (concentration, pH, mass in grams, moles, energy).

### 3. `surface/lexicon.py` — the words a student writes

Per role, per subject, tagged by register:

    "concentration": {"exam": ("molarity", "molar concentration"),
                      "lab": ("concentration of the solution",),
                      "classroom": ("how many moles per liter",)}

Held out by *structure*, not by word: the transfer cohort measures structures
never trained on, so the lexicon can stay in training without flattering the
measurement. `vary.py`'s two held-out lexical choices stay as they are; they
measure a different axis.

### 4. `surface/numbers.py` — number and unit renderers

Exact fraction, decimal, percent, scientific notation, mixed units (mL against
L), and per-slot forbidden formats. The last one already has a live failure: the
grounding rule matches digits, so `str(0.000063)` renders as `6.3e-05` and the
call and the question disagree unless small values are formatted deliberately.
A slot's `forbidden` list is where that gets recorded once, instead of being
rediscovered by every author.

### 5. `gen_wording.py` — validator, ledger, split

For every generated row, in order:

1. it solves (`relations.RELATIONS[name].solve(*args)` returns without raising);
2. every argument is grounded in its own text, by the runtime's rule, ported
   rather than re-implemented (`pipeline_args_grounded` semantics);
3. grammar: no unfilled placeholder, no doubled space, sentence re-renders to
   the same normalised structure id;
4. novelty: the normalised sentence is not within a Jaccard threshold of an
   existing row for the same relation — *structure* novelty is what the ledger
   counts, row novelty is a cheap duplicate guard;
5. decidability: the ask names the answer quantity and every same-dimension pair
   in the row's structure is named.

The **coverage ledger** is the output that stops the illusion: distinct
normalised structures per relation, rows per relation, held-out structures, and
distinct lexicon variants. A relation with 400 rows from 3 structures is reported
as 3. The ledger is generated and checked in a gate (`--check`), like every other
generated artifact in this repository.

**Split by structure.** `sha256(relation|structure_id)` decides: one structure in
five is held out whole, so its rows are eval and never trained. The hand-written
split's per-relation rule stays exactly as it is; the new file only ever adds its
own rows.

## What this does not buy

It is worth writing down before anyone measures it. Round four of hand-written
data bought **1.3 points with a 0.9-point error bar**, and the generated gate
reads 90.7% while the same weights read 72.5% on hand-written rows and about 10%
on real exam questions. Generated breadth inflates the one metric that is easiest
to inflate.

So the honest wins, in the order this system delivers them:

1. **Distribution realism** — sentences that read like the exam bank, which is
   the gap between 72.5% and 10%, and no amount of volume closes it.
2. **Precise minimal pairs** — the confusable pairs get same-numbers,
   one-word-different rows, which is the cheapest known lever on the 9.2%
   wrong-relation rate.
3. **A leakage-proof eval** — structures held out whole, so a rising number
   means transfer rather than memorisation.
4. **Authoring speed** — one spec per relation instead of ten sentences plus a
   dozen rewrites, and a ledger that says what the bank actually contains.

A rising generated gate is not evidence for any of them.

## How it gets judged

1. **Transfer cohort** — accuracy on structures never trained on, before and
   after this corpus replaces the hand-written one at equal row count.
2. **Hand-written accuracy** — the 2,150-row eval split, two seeds.
3. **All three course floors** — a course below its floor is a fail.
4. **The confusion-pair table** — the eight pairs with counts, before and after,
   because the aggregate moved 1.2 points inside noise last time and told us
   nothing.

## Phasing

- **Phase 1, this deliverable.** Chemistry, end to end: specs for all 48
  relations, the clause library, the lexicon, the renderers, the validator, a
  generated corpus in a new fixture file, and the ledger. Plus the eight
  confusion pairs as contrast frames. No retrain; the Captain schedules it.
- **Phase 2.** The transfer cohort wired into the gate, and the ledger as a
  `--check` gate so a relation cannot silently lose structure.
- **Phase 3.** Other subjects, one at a time, reusing the grammar and adding
  only their clause library and lexicon.

## Decisions I want argued

1. **Naming is a structural requirement, not an inference.** If a sentence does
   not name a same-dimension role, the row is refused. That is stricter than the
   current corpus and it will make some perfectly ordinary sentences
   unrepresentable ("mix 0.2 mol into 0.5 L" — is it the amount of solute or the
   amount of solvent?). I think that is right, because NDLE-21 measured what
   happens when the model is asked to infer it.
2. **`carnot` and `carnot_efficiency` are one quantity in two formats.** The
   system will generate a format discriminator for them, but the real fix is a
   contract question — collapse them, or make the output format an argument.
   That is out of scope here and I am reporting it rather than deciding it.
3. **Structure holdout at one in five.** Large enough to measure, small enough
   that a relation with 40 structures still trains on 32. If the transfer cohort
   is too noisy at that size, the fix is more structures per relation, not a
   different fraction.
