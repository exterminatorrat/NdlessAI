# Tier 1 plan: breadth

Everything needed to take the system from 11 relations to roughly 60, covering
a high-school curriculum, without changing the model architecture.

## 1. What a relation costs

Four files change per relation. Nothing else.

| File | Change | Lines |
|---|---|---|
| `relations.py` | one class: `gen()` and `solve()` | ~15 |
| `surface.py` | 8-14 phrasings in `TEMPLATES` | ~12 |
| `runtime/solvers.c` | one `solve_*` function plus a dispatch branch | ~87 |
| `runtime/test_solvers.c` | generated cases, not hand-written | 0 |

The C side is the expensive part because it must reproduce Python's exact
output string, including the four-significant-digit `dec()` format and exact
rationals. That is what makes the port verifiable, and it is not negotiable.

## 2. Architecture decisions

**Numeric arguments only.** Every relation takes integers or decimals. This
keeps the call format, the guard, and the C solvers uniform. Chemistry that
would need formula parsing (balancing equations) is deferred to Tier 3, where
an expression parser exists.

**One model, more tools.** The model classifies among relations. Going from 11
to 60 is a harder classification problem but not a different one. Vocabulary
has room: 61 relations leaves 190 BPE merges for text.

**Exact where possible.** Relations that can return exact rationals do
(`exact=True`); physical quantities use `dec()` at four significant digits.
This split already exists and must be preserved.

**The guard stays.** Every argument must appear in the question, sign-aware.
New relations inherit this for free because it operates on the call string.

## 3. The relation catalogue

Six batches. Each is independently shippable and independently verifiable.

### Batch A: trigonometry (8 relations)

| Name | Args | Computes |
|---|---|---|
| `trig_eval` | fn, angle | sin/cos/tan of degrees |
| `trig_inverse` | fn, value | arcsin/arccos/arctan in degrees |
| `pythagoras` | a, b | hypotenuse from two legs |
| `right_triangle` | opp, adj | angle and hypotenuse from SOH CAH TOA |
| `law_sines` | a, A, B | side b from two angles and one side |
| `law_cosines` | a, b, C | side c from two sides and included angle |
| `sinusoid` | A, B, C, D | amplitude, period, phase of A*sin(Bx+C)+D |
| `arc_length` | r, theta | arc length and sector area |

### Batch B: logs and exponentials (7 relations)

| Name | Args | Computes |
|---|---|---|
| `log_eval` | base, x | log base b of x |
| `log_expand` | a, b | log(ab) as sum, log(a/b) as difference |
| `change_base` | base, x, newbase | log rewritten in another base |
| `compound_interest` | P, r, n, t | A = P(1+r/n)^(nt) |
| `continuous_growth` | P, r, t | A = Pe^(rt) |
| `half_life` | initial, half_life, t | remaining amount |
| `doubling_time` | rate | time to double at a growth rate |

### Batch C: algebra (8 relations)

| Name | Args | Computes |
|---|---|---|
| `cubic_solve` | a, b, c, d | real roots of a cubic with rational roots |
| `factor_quadratic` | a, b, c | factored form from roots |
| `complete_square` | a, b, c | vertex form and vertex |
| `arith_seq` | a1, d, n | nth term and sum |
| `geom_seq` | a1, r, n | nth term and sum |
| `system3` | 9 coefficients | 3x3 linear system by elimination |
| `abs_value_eq` | a, b, c | solve |ax+b| = c |
| `linear_ineq` | a, b, c | solve ax+b < c, report interval |

### Batch D: physics (12 relations)

| Name | Args | Computes |
|---|---|---|
| `kinetic_energy` | m, v | 0.5*m*v^2 |
| `potential_energy` | m, h, g | mgh |
| `work` | F, d, theta | Fd*cos(theta) |
| `power` | W, t | W/t |
| `momentum` | m, v | mv |
| `newton2` | m, a | F = ma |
| `circular_accel` | v, r | v^2/r |
| `wave_speed` | f, lambda | f*lambda |
| `specific_heat` | m, c, dT | q = mc*dT |
| `ohm_v` | I, R | V = IR |
| `ohm_power` | I, V | P = IV |
| `free_fall` | h, g | v = sqrt(2gh) |

### Batch E: chemistry (10 relations)

| Name | Args | Computes |
|---|---|---|
| `molarity` | n, V | mol/L |
| `ph` | H | -log[H+] |
| `poh` | OH | -log[OH-] |
| `ph_from_poh` | poh | 14 - pOH |
| `percent_yield` | actual, theoretical | actual/theoretical * 100 |
| `boyles` | P1, V1, V2 | P2 = P1V1/V2 |
| `charles` | V1, T1, T2 | V2 = V1T2/T1 |
| `combined_gas` | P1,V1,T1,P2,T2 | V2 |
| `density` | m, V | m/V |
| `stoichiometry` | moles_given, ratio | moles of product |

### Batch F: units and constants (5 relations)

| Name | Args | Computes |
|---|---|---|
| `unit_convert` | value, from_id, to_id | length, mass, volume, temperature |
| `sig_figs` | value, n | rounded to n significant figures |
| `percent_error` | measured, accepted | |m-a|/a * 100 |
| `scientific_notation` | mantissa, exponent | decimal form |
| `average` | a, b, c, d | mean of up to four values |

**Total: 50 new relations, 61 overall.**

## 4. Training data

**Volume.** 200,000 rows per full build, as now. With 61 relations that is
about 3,300 rows per relation, which is enough: the current 11 relations train
on about 18,000 each and converge in 6,000 steps. If accuracy drops, raise to
400,000 rather than adding steps.

**Phrasing.** 8-14 per relation, every fifth held out. This is the single
biggest accuracy lever measured so far: 4 phrasings gave 35% held-out, 8-12
gave 57%. Do not ship a relation with fewer than 8.

**Distractors.** 50% of rows carry one plausible same-unit extra quantity.
This is what teaches the model to ignore irrelevant numbers.

**Splits.** Train, val, and held-out phrasing, disjoint by seed and by
template. `make_dataset.py` already does this; the held-out check is a test.

## 5. Sources

Formulas are standard and are not the risk. The risk is *phrasing*, which is
why the surface bank matters more than the formula.

| Domain | Reference for formulas | Reference for phrasing |
|---|---|---|
| Algebra, trig, logs | any standard precalculus text | textbook exercise wording |
| Physics | AP Physics 1/2 formula sheet | AP free-response phrasing |
| Chemistry | AP Chemistry formula sheet | lab-report phrasing |
| Units | SI brochure | common conversion questions |

Do not copy text from copyrighted sources. Write phrasings in the style of
textbook exercises; the formulas themselves are not copyrightable.

## 6. Verification gates

A batch is done only when all of these pass. No exceptions.

| Gate | Threshold | Command |
|---|---|---|
| Python golden values | hand-computed, per relation | `pytest tests -k golden` |
| C solvers match Python | 0 mismatches, byte-identical | `./test_solvers` |
| Tokenizer round-trip | 100% | `./test_bpe` |
| Fixed-point parity | argmax agrees, corr > 0.999 | `./test_fixed` |
| Guard cases | all pass | `./test_guard` |
| ARM cross-compile | warning-free | `sh crosscheck.sh` |
| Held-out phrasing | >= 80% exact | `evaluate.py --data data/held_out.jsonl` |
| Wrong relation | <= 5% | same |
| On hardware | one correct answer per batch | manual |

The held-out gate is the one that matters. A batch that scores 100% on
in-distribution and 40% on held-out has memorised phrasing, not learned it.

## 7. Subagent workflow

Each batch is one subagent, with a disjoint write set. They cannot collide
because each owns different relation names.

**Per-batch prompt must state:**

- The exact relation names and argument order
- That `solve()` must return the same string the C solver will produce
- The `dec()` and `fmt()` formatting rules, with examples
- That `surface.py` needs 8-14 phrasings, every fifth held out
- That the C solver must be byte-identical to Python
- The verification commands and that they must pass before reporting

**Parallelism.** Batches A-F are independent. Run three at a time: the sandbox
has 2 CPUs and each subagent compiles C, so more than three thrashes.

**Integration.** After each batch lands, the parent runs the full gate table
and regenerates the dataset. Do not merge a batch that fails a gate.

## 8. The loop

    for each batch:
        1. spawn subagent with the batch spec
        2. parent verifies: run the gate table independently
        3. if a gate fails, send the failure back to the subagent
        4. when all gates pass, commit the batch
        5. after all batches: regenerate dataset, retrain, evaluate
        6. if held-out < 80%, add phrasings, not parameters
        7. build .tns, test one question per batch on hardware

Step 2 is not optional. Every serious bug in this project was found by the
parent re-running a check the subagent said had passed.

## 9. Known risks

**Classification gets harder.** 61 relations means more confusable pairs
(`kinetic_energy` vs `potential_energy`, `ph` vs `poh`). If wrong-relation
rate exceeds 5%, the fix is more distinctive phrasings, not a bigger model.

**C solver drift.** Each solver must reproduce Python's string exactly. The
`dec()` rounding and rational formatting are the usual source of mismatch.
Generate the C test cases from Python; never hand-write expected values.

**Vocabulary pressure.** 61 relations uses 66 special ids, leaving 190 merges.
Fine. At 200 relations it would be 51 merges, which is too few — that is the
point to reconsider the tokenizer.

**Training time.** 200k rows at 61 relations is the same token count as now,
so training stays under an hour on a laptop GPU.

## 10. Definition of done

- 61 relations, all gates green
- Held-out phrasing >= 80%, wrong relation <= 5%
- One `.tns` that answers a question from every batch on hardware
- `ROADMAP.md` updated with what was actually achieved
