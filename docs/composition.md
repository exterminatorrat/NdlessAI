# Composition: which relations chain, and how far that gets a real question

A chain is several calls where each step may use the previous step's answer:
`CALL mixing_concentration 10 0.002 10 0 ; CALL poh <1> ; CALL ph_from_poh <2>`
answers 11. The runtime could already execute that before this page existed
(`runtime/plan.c`), and since #163 it can also be handed one by the decoder and
show every step on the screen. What was never established is which relations
*should* be chained — that is a generation question, because the runtime passes a
bare magnitude to the next step and never looks at its unit.

This page answers two things with one derivation: how big the composition graph
is, and how many of the bank's second-computation rows a chain of catalogue
relations can actually express.

## The rule, derived

`stem-model/compose.py` builds the graph from the tree's own declarations. Three
sources, each with one job:

| Source | What it says | Coverage |
| --- | --- | --- |
| the reference solver in `relations/` | the answer string the app shows, e.g. `3.92 N` — so it states a step's answer *and* whether it is one number at all | 178 of 178 |
| `runtime/unitstable.h` | the unit each argument is phrased with | 130 of 178 |
| `surface/spec.py` | a dimension for every slot and answer | 74 of 178, all 54 chemistry relations included |

An edge `S -> R` means R has a slot that can take S's answer, and it is admitted
only when all three hold:

1. **S answers with one number.** Measured, not assumed: the reference's answer
   is rejected when it holds `=` or `,`, which is the same test `plan.c`'s
   `magnitude_of` applies before a chain may continue. 159 of 171 relations pass.
2. **The units match** — equal after normalising the two spellings the tree uses
   (`M` and `mol/L`, `percent` and `%`), or related by a factor in `FACTORS`
   (kJ↔J, mL↔L, g↔kg, nm↔m, …). Where a slot states no unit at all, the edge may
   instead be carried by dimensions, which is how `pOH` reaches `ph_from_poh`.
3. **The dimensions agree** where both sides declare one, or one refines the
   other in `REFINES` — a wavelength is a length and an enthalpy change is an
   energy, and in both cases the *slot* says which of the two it wants.

The factor table and the two refineries are the only hand-written lists here,
because a conversion and a refinement are facts about units rather than about a
relation. Everything else is read from the tree.

### What the derivation found

```
$ uv run --with sympy --with numpy python compose.py
relations in the catalogue            178
answers that are a single number      166
relations with a declared slot unit   130
composition edges                    1084
relations that can open a chain: 99
```

The cross-check disagreed on two edges until the phrasing was fixed: `moles`
and `stoichiometry` answer in `mol`, and `stoich_mass`'s fourth slot — the mole
ratio — was *phrased* as "{ratio} mol", so the unit table called a dimensionless
ratio a number of moles. Rewording the templates fixed the metadata rather than
the graph: the slot states no unit now, no amount in moles fits it, and the
report reads zero. That is the whole value of deriving rather than declaring —
the defect surfaced as a disagreement, not as a wrong chain.

The graph is a *reachability* statement, not a plan: from the busiest source,
`calorimetry_mass`, one call reaches 1 relation, two reach 23, three 70 and four
106. Chain length is cheap; what limits a chain is what it can start from.

### `min_of`: a comparison is a call, not control flow

The limiting rows are the reason it exists. A row that gives two candidate
amounts and asks how much product the reaction makes needs the *smaller* of two
computed amounts, and there was no relation whose answer was that smaller number
— the comparison lived in a student's head, which is the one place the runtime
cannot read it. So `min_of a b` is a relation like any other: two magnitudes in,
the smaller one out.

That is a deliberate trade. The alternative is a plan-level *branch* — "stop if
a < b" — and a branch is control flow: the decoder would have to emit a jump, the
pane would have to show a step that did not run, and the grounding guard would
have to reason about a path it never took. Making the comparison a relation
instead keeps every step a pure function of the numbers before it, which is the
shape the whole chain path is built on: no branches, no loops, one answer per
step. The cost is that `min_of` answers with a bare magnitude — it compares two
numbers and cannot know what they were counting — so a chain that ends on it
prints `20` where the question wants `20 mol`, and the unit is the reader's to
carry. Of the three rows that compute and ground, this is the only one whose
answer loses its unit; `pH` and `Kc` are dimensionless.

## The 23 second-computation rows, measured

NDLE-23 hand-read 70 mechanically gated MCQ rows and labelled 23 of them
`two_step`. `fixtures/chain_rows.txt` reads those 23 again, for a different
question: how many calls of the catalogue's own relations compute the row, and
what stops the ones that are not.

```
$ uv run --with sympy --with numpy python compose.py --rows fixtures/chain_rows.txt
```

| Verdict | Rows | What it means |
| --- | --- | --- |
| `chain-1` | 6 | one call answers it, four of them waiting for the row's own arithmetic |
| `chain-2` | 6 | two calls of existing relations compute the row |
| `chain-3` | 6 | three calls |
| `chain-4` | 1 | four calls: two `moles_from_molarity`, then the mixture, then `molarity` |
| `chain-5` | 1 | five calls, the most the runtime plans (`PLAN_MAX_STEPS`) |
| `blocked` | 3 | no chain of catalogue relations computes it |

### The curve, as each family of relations landed

The batch was authored in the order the blocked counts implied, and the point of
keeping the progression is that the batch's value is a curve rather than a claim.

| Batch state | Edges | 1-call | 2-call | 3-call | 4 or 5 | Blocked |
| --- | --- | --- | --- | --- | --- | --- |
| before any relation | 921 | 2 | 3 | 1 | 0 | 17 |
| `+ moles_from_molarity` (`n = c·V`) | 944 | 2 | 4 | 2 | 0 | 15 |
| `+ min_of` and `kc_from_ice` | 960 | 4 | 4 | 4 | 0 | 11 |
| `+ ions_from_mixture` and `ion_from_ksp` | 1005 | 5 | 5 | 4 | 2 | 7 |
| `+ excess_moles` (the net amount left over) | 1039 | 5 | 6 | 5 | 4 | 3 |
| `+ excess_concentration` (the excess over the combined volume) | 1084 | 6 | 6 | 6 | 2 | 3 |

**The honest headline: seven rows of the 23 are chains the runtime computes,
grounds and would display** (u03-187, u04-064, u04-050, u07-016, u08-015,
u08-016, u08-068), and twenty of the 23 are chains at all — up from six before
this batch. The decoder cannot write any
of them yet, because a relation's name reaches the plan parser only through
`reltable.h`, which the retrain regenerates; that is what a chain corpus and a
retrain are for.

Of the seventeen chains that are not answered today, the split matters more than
the count. **Ten are refused on a literal**, and those refusals fall into four
shapes: four carry a ratio the question writes as two coefficients (`1.5`, `3`,
`0.6667`) rather than as the one number the call needs, two write a count inside
a formula (`CaCl2`, `Ba(OH)2`) instead of as a numeral, two write a number the
guard compares too strictly (`0.0300` against `0.03`, `8.0` against `8`), and two
state a number in exponent notation (`4.0 x 10-8`, `1.0 x 10-3`) that no literal
reproduces.

**Two are not literal failures at all and wait only on the retrain:** u04-042 and
u04-046 write every one of their numbers exactly as the question does, and the
only thing missing is that `moles_from_molarity` is not in `reltable.h`, so the
unit stack cannot see its slots and leaves `20. mL` as twenty litres.

**And one was a defect in the unit stack, not in this batch**, fixed in #169: a
number written `100.` had its unit read from the period, so u04-066 answered
`0.0001 mol/L` where the key is `0.1`. With that fix the row is a one-call chain
the runtime answers end to end.

The remaining four are arithmetically computable and refused on the total volume
`excess_moles` cannot supply, which is the section below. The other three are not
a chain-length or a grounding problem at all.

### The chains, and what happens to them

Run step by step with each row's own numbers, written as the question writes
them. A volume in millilitres is converted into the slot's litres by
`quantity_check_call`, not by hand — which for the rows below needs
`moles_from_molarity` to be in `reltable.h`, so the arithmetic column is what the
solver sequence produces and the notes say where a conversion is still pending:

| Row | Chain | Answer | Bank key |
| --- | --- | --- | --- |
| u03-187 | `mixing_concentration 10 0.002 10 0 ; poh <1> ; ph_from_poh <2>` | `11` | 11 |
| u04-039 | `stoich_mass 12.3 123 32 1.5 ; percent_yield 3.2 <1>` | `66.67%` | 67% |
| u04-092 | `stoich_mass 70 35 8 3 ; percent_yield 8 <1>` | `16.67%` | 25% |
| u01-066 | `moles 14.2 284 ; stoichiometry <1> 5` | `0.25 mol` | 0.250 |
| u04-038 | `moles_from_molarity 20 0.20 ; stoichiometry <1> 0.75` | `0.003 mol` | 0.0030 |
| u04-046 | `moles_from_molarity 14.0 0.050 ; stoichiometry <1> 5 ; molarity <2> 25.0` | `0.14 mol/L` | 0.14 |
| u04-064 | `stoichiometry 6.0 4 ; stoichiometry 20 1 ; min_of <1> <2>` | `20 mol` | 20 |
| u04-109 | `stoichiometry 0.40 0.5 ; stoichiometry 0.40 0.6667 ; min_of <1> <2>` | `0.2 mol` | 0.20 |
| u07-001 | `kc_from_ice 0.03 0.005 2` | `12.5` | 12.5 |
| u07-016 | `kc_from_ice 0.0688 0.0133 2` | `7.468` | 7.47 |
| u04-049 | `ions_from_mixture 0.10 1 0.10 2` | `0.3 mol` | 0.30 |
| u07-105 | `ion_from_ksp 0.00000004 0.000001 2 ; moles_from_molarity <1> 1.00` | `0.2 mol` | 0.2 |
| u03-148 | `moles_from_molarity 0.25 40.0 ; moles_from_molarity 0.15 60.0 ; ions_from_mixture <1> 1 <2> 2 ; molarity <3> 100.` | `0.28 mol/L` | 0.28 |
| u04-042 | `moles_from_molarity 0.20 20. ; moles_from_molarity 0.10 50. ; stoichiometry <1> 5 ; stoichiometry <2> 2 ; min_of <3> <4>` | `0.01` | 0.010 |
| u04-050 | `excess_concentration 20.0 0.200 30.0 0.400` | `0.16 mol/L` | 0.16 |
| u08-015 | `excess_concentration 150.0 0.10 50.0 0.20 ; poh <1> ; ph_from_poh <2>` | `12.4` | 12.4 |
| u08-016 | `excess_concentration 10 1 10 1.2 ; poh <1> ; ph_from_poh <2>` | `13` | 13 |
| u08-068 | `excess_concentration 200 2.0 500 1.0 ; ph <1>` | `0.845` | 0.85 |

u04-092 is shown with the ratio that makes its arithmetic reproduce the key: the
row gives no equation, so the ratio is the solver's to know and the bank's key
implies 2 rather than the 3 the formula suggests.

Then the guard that runs before any answer is shown, `grounding_arg_in_text`,
asked every literal of every chain against its own row's text — with the chain
written the way the question writes it, not pre-converted. Measured, not assumed:

| Row | Literals | What stops it |
| --- | --- | --- |
| u03-187 | all stated | **nothing — answered** |
| u04-064 | all stated (`6.0`, `4`, `20`, `1`; the coefficients are numerals in the equation) | **nothing — answered**, and the answer prints `20` without its unit because `min_of` compares magnitudes |
| u07-016 | all stated | **nothing — answered** |
| u04-042, u04-046 | all stated | nothing literal: `moles_from_molarity` is not in `reltable.h`, so its slots are invisible to the unit stack and `20. mL` stays twenty litres. **The retrain closes these.** |
| u04-066 | all stated | nothing literal: a number written `100.` had its unit read from the period. **Fixed in #169**, after which this row is answered |
| u01-066, u04-038, u04-039, u04-109 | ratio missing | the ratio (`5`, `0.75`, `1.5`, `0.5`, `0.6667`) is not a number in the sentence |
| u04-092 | ratio and form | the ratio `3`, and the mass is written `8.0 g` where the call needs `8` |
| u03-148, u04-049 | count missing | the ion count (`2` in `Ba(OH)2`, `2` in `CaCl2`) is written inside a formula, not as a numeral |
| u07-001 | form and count | the sentence writes `0.0300` and `0.00500` where the call needs `0.03` and `0.005`, and the ratio `2` |
| u07-105, u05-004 | exponent | `4.0 x 10-8`, `1 x 10-6`, `1.0 x 10-3`: no literal reproduces them |

Two things this table taught, both of them corrections to an earlier reading of
it:

**A millilitre written against a litre is not a wall.** The first version of this
page listed five rows as refused because "the volume is `0.020` L and the
sentence writes `20. mL`" — but that was this page's own hand-conversion, not the
runtime's behaviour. The pipeline grounds the number **as written** and only then
rewrites it in the slot's unit, which is what #162 and #167 built. A chain has to
carry `20`, not `0.020`: the conversion is the unit stack's job, and a chain that
does it by hand is a chain the guard refuses for a reason that does not exist.

**What remains is a literal wall, and it is about shape rather than units.** A
ratio is written in a balanced equation as two coefficients (`2 KClO3 → … 3 O2`),
not as their quotient, so a chain cannot carry it as a literal even when both
coefficients are in the text — which is why u04-064 grounds: its equation
writes `4` and `1` as numerals and the chain uses them directly.

### What blocks the 3

| Blocker | Rows |
| --- | --- |
| the row needs six calls, one past `PLAN_MAX_STEPS` | 1 |
| the value the row needs lives in a graph, not in its text | 1 |
| one Kp whose products are two species, which needs a power no relation computes | 1 |

### Two shapes for the same arithmetic, and why both are here

`excess_moles v1 c1 v2 c2` is `|c1*V1 - c2*V2|`: the net amount left after a
one-to-one reaction, on the four rows that wanted a subtraction. It computes
them exactly — `0.008`, `0.005`, `0.002`, `0.100` mol — and every one of them
then fails the guard one step later, because the concentration step divides by
the total volume and **the total volume is a sum the sentence never writes**:
"150.0 mL of 0.10 M KOH with 50.0 mL of 0.20 M HBr" states 150.0 and 50.0 and
never 200.0.

`excess_concentration v1 c1 v2 c2` is that same arithmetic over `V1 + V2`, and
it closes all four: `0.16`, `0.025`, `0.1` and `0.1429` mol/L, which are the
bank's keys, grounded, with the pH steps after them. It is the mirror of
`mixing_concentration`, which already takes those four arguments — so the
catalogue's own idiom, not an invention.

The rule the pair settles, and the one to apply to the next shape: **a relation
is justified when its arguments are numbers the sentence states.** `V1 + V2` is
not one, so a relation that needs it cannot be grounded however exact its
arithmetic; `c1, V1, c2, V2` are all four in the text, so the relation that takes
them can be. That is a stronger test than "the arithmetic is right", and it is
the one the four rows actually failed.

## What this means before a chain corpus is generated

1. **Chain length is not the constraint; the catalogue's shapes are.** Four calls
   reach 106 relations, and the measured population tops out at five calls with a
   single row. A corpus of four- and five-call chains would teach shapes the bank
   asks for rarely: twenty of the 23 rows are chains, and eleven of those need
   three calls or fewer.
2. **A chain corpus over today's relations would be mostly synthetic, and that is
   the honest limit.** Three rows of 23 are the evidence it can reach today, and
   the binding constraint is the *literal* wall rather than the relation set: ten
   chains are arithmetically exact and refused on a literal, two more wait only
   on `reltable.h` being regenerated, one was a unit-stack defect now fixed in
   #169, and four are refused on a total volume no sentence states.
3. **The subtraction is built and it is one step short.** `excess_moles` gives
   the net amount for four rows at once, and all four then stall on the *total*
   volume, which their sentences never write. The shape that closes them is the
   same arithmetic divided by the sum of the two volumes — the mirror of
   `mixing_concentration`, which already takes those four arguments.
4. **The graph checks units, never roles.** `molarity` answers `mol/L` and
   `mixing_concentration` has a slot in `mol/L`, so the graph admits
   `molarity -> mixing_concentration`; a question that needs `n = c·V`,
   `n` then a ratio then `c`, needs calls in an order no single edge describes,
   and u03-148 needs two independent `moles_from_molarity` calls where the graph
   counts one hop. Every generated chain has to be checked against its own
   question by hand or by the reference, never by the graph alone.
5. **A relation's name is a decoded token, so a new relation is invisible until
   the retrain.** `plan_execute` accepts a step only when `relation_named` finds
   the name in the generated `reltable.h` (171 relations at the time of this
   batch). Measured: `CALL kc_from_ice 0.03 0.005 2 ; CALL moles 1 1` is refused
   with rc=-1, while `CALL beer_lambert 100 0.5 1 ; CALL moles 1 1` runs. The
   solvers answer the new names directly (`solvers_solve` dispatches on the
   string), so the arithmetic is ready and the naming is not. That is not a
   defect to work around: the table *must* be regenerated with `bpe.h`, or the
   vocabulary and the relation ids disagree.
6. **An answer that drops its unit hides its own edge.** `ion_from_ksp` was
   written returning `0.2` while its slot declared `mol/L`; the graph found no
   edge from it to `moles_from_molarity`, because a chain carries the printed
   unit. Printing `0.2 mol/L` created the edge. The lesson is that a solver's
   answer string is metadata the graph reads, not decoration: the same defect
   costs u04-064 its displayed unit at the end of a chain.

## Limits of this page

- The graph's dimension coverage is 72 of 176 relations, complete for chemistry
  and partial for physics. Units cover 128. Where neither declares anything, an
  edge is missing rather than guessed, so the edge count is a floor.
- The 23-row reading is one person's read of each row's arithmetic, with the
  chain verified against the runtime and the bank key where a key exists. The
  labels are ids and relation names in `fixtures/chain_rows.txt`; no question
  text is stored here, in the fixture, or in the pull request.
- The reading was done against the ingested bank (gitignored, `corpora/apchem`)
  with `apchem_ingest.py --no-ocr`, and every chain was run step by step on the
  host build through the pipeline's own decision path — grounding on the call as
  decoded, then `quantity_check_call`, then the solver. The two longest were also
  offered to `plan_execute`, which refuses them by name until `reltable.h` is
  regenerated. Nothing was retrained and no weights were used.
- **A row a person writes by hand carries that person's model of the runtime, and
  the runtime is the authority.** This page was wrong twice for the same reason:
  the chains were written with the arithmetic already done (`0.020` where the
  question writes `20. mL`), and the guard's refusal of the converted literal was
  read as a property of the runtime rather than of the input. The same mistake in
  the other direction produced the trailing-period defect — a number copied out of
  the sentence, where the sentence's own notation (`100.`) defeated the unit
  reader. Before drawing a conclusion from a refusal, re-run it with the values
  copied from the source rather than from the working.
- Chains are written with the literals the question writes, never pre-converted:
  `moles_from_molarity 20 0.20` for a row that states `20. mL`, not
  `moles_from_molarity 0.020 0.20`. The first version of this page did the
  conversion by hand and then reported the guard's refusal as a wall.
- `ion_from_ksp`'s `gen` draws a Ksp of 1e-4 or more, the smallest value the
  wording layer writes without an exponent. The solvers are exact for any
  positive Ksp the runtime parses — the bank's own row, 4.0 x 10-8, computes to
  0.2 mol/L — so the gap is in writing the question, not in answering it.
