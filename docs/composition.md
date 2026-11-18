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
| the reference solver in `relations/` | the answer string the app shows, e.g. `3.92 N` — so it states a step's answer *and* whether it is one number at all | 171 of 171 |
| `runtime/unitstable.h` | the unit each argument is phrased with | 124 of 171 |
| `surface/spec.py` | a dimension for every slot and answer | 67 of 171, all 48 chemistry relations included |

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
relations in the catalogue            171
answers that are a single number      159
relations with a declared slot unit   124
composition edges                     921
cross-check disagreements               2
relations that can open a chain: 93
```

Two edges the units allow are refused by the dimensions, and both are the same
defect rather than two: `moles` and `stoichiometry` answer in `mol`, and
`stoich_mass`'s fourth slot — the mole ratio — is *phrased* with `mol` in the
unit table while `spec.py` declares it a `ratio`. The unit table is describing
the quantity the sentence mentions rather than the slot's own dimension. Left
uncorrected it would let a chain divide by a number of moles where a ratio
belongs, so the graph refuses the edge and this note is the report.

The graph is a *reachability* statement, not a plan: from the busiest source,
`calorimetry_mass`, one call reaches 1 relation, two reach 23, three 68 and four
102. Chain length is cheap; what limits a chain is what it can start from.

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
| `chain-2` | 3 | two calls of existing relations compute the row |
| `chain-3` | 1 | three calls |
| `chain-1` | 2 | labelled two-step, but one call answers it |
| `blocked` | 17 | no chain of catalogue relations computes it |
| four calls | 0 | nothing in this population needs four |

**The honest headline: one row of the 23 is a chain the runtime computes,
grounds and would display.** The decoder cannot write one yet — that is what a
chain corpus and a retrain are for — but everything behind the decode is ready
for that one row. Three more are two-call chains whose arithmetic is exactly
right and whose ratio is an argument the grounding guard refuses (see below).
The other 19 are not a chain-length problem at all.

### The four chains, and what happens to them

Run through `plan_execute` on the merged runtime, with each row's own numbers:

| Row | Chain | Answer | Bank key |
| --- | --- | --- | --- |
| u03-187 | `mixing_concentration 10 0.002 10 0 ; poh <1> ; ph_from_poh <2>` | `11` | 11 |
| u04-039 | `stoich_mass 12.3 123 32 1.5 ; percent_yield 3.2 <1>` | `66.67%` | 67% |
| u04-092 | `stoich_mass 70 35 8 3 ; percent_yield 8 <1>` | `16.67%` | 25% |
| u01-066 | `moles 14.2 284 ; stoichiometry <1> 5` | `0.25 mol` | 0.250 |

u04-092 is shown with the ratio that makes its arithmetic reproduce the key: the
row gives no equation, so the ratio is the solver's to know and the bank's key
implies 2 rather than the 3 the formula suggests.

Then the guard that runs before any answer is shown, `pipeline_args_grounded`,
asked the same four chains against their own row text:

| Row | Grounded |
| --- | --- |
| u03-187 | yes |
| u01-066, u04-039, u04-092 | no — the ratio (`5`, `1.5`, `3`) is not a number in the sentence |

That is the second wall behind the first. A ratio is written in a balanced
equation as two coefficients (`2 KClO3 → … 3 O2`), not as their quotient, so a
chain cannot carry it as a literal even when both coefficients are in the text
and the arithmetic is a two-call chain. The one row that grounds is the one
whose chain needs no ratio at all.

### What blocks the 17

| Blocker | Rows | Rows it alone would unlock |
| --- | --- | --- |
| no relation gives moles from concentration and volume | 9 | 2 |
| the row needs a comparison between two candidates (limiting reagent) | 3 | 0 |
| arithmetic between two relations (an ICE table's subtraction) | 3 | 0 |
| ions per formula unit of a *mixture* | 1 | 0 |
| no relation gives solubility from Ksp | 1 | 0 |

Naming a relation that answers `n = c·V` is the cheapest capability on this list,
and it is worth two rows: one titration, and one that also has to multiply by a
reaction ratio. Every other blocker is either an arithmetic step a chain of calls
cannot express (`0.030 − 0.010`) or a decision (`which reactant runs out`), and
neither is a chain-length problem.

## What this means before a chain corpus is generated

1. **Chain length is not the constraint; the catalogue's shapes are.** Four calls
   reach 102 relations, and nothing in the measured population wants more than
   three. A corpus of four-call chains would teach a shape the bank never asks
   for.
2. **A chain corpus over today's relations would be mostly synthetic.** Three
   rows of 23 are the evidence it can reach; the rest of the value is in the
   relations that do not exist yet.
3. **Two shapes are worth building before the corpus, not after.** A relation for
   moles from concentration and volume, and a way to state a stoichiometric ratio
   as the two coefficients the equation writes (the pattern `rate_factor`
   already uses for rates) — the second is what makes the three working 2-call
   chains *groundable*, and grounding is the guard that keeps a wrong number off
   the screen.
4. **The graph checks units, never roles.** `molarity` answers `mol/L` and
   `mixing_concentration` has a slot in `mol/L`, so the graph admits
   `molarity -> mixing_concentration`; a question that needs `n = c·V` needs the
   reverse, which no relation answers. Every generated chain has to be checked
   against its own question by hand or by the reference, never by the graph
   alone.

## Limits of this page

- The graph's dimension coverage is 67 of 171 relations, complete for chemistry
  and partial for physics. Units cover 124. Where neither declares anything, an
  edge is missing rather than guessed, so the edge count is a floor.
- The 23-row reading is one person's read of each row's arithmetic, with the
  chain verified against the runtime and the bank key where a key exists. The
  labels are ids and relation names in `fixtures/chain_rows.txt`; no question
  text is stored here, in the fixture, or in the pull request.
- The reading was done against the ingested bank (gitignored, `corpora/apchem`)
  with `apchem_ingest.py --no-ocr`, and the four chains were run on the host
  build. Nothing was retrained and no weights were used.
