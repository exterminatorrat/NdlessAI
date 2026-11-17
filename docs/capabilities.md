# Capabilities inventory

Every capability the project could add, grouped by the surface it touches, with
the work it costs and whether it needs a retrain. This is a catalogue, not a
plan: `ROADMAP.md` orders work by value, and this page makes sure nothing is
forgotten.

Nothing here is scheduled. An entry is a candidate, and several entries are
recorded deliberately as "not worth doing" so the reason survives.

Four wording and selection lines were built far enough to measure and closed by
their own numbers on 2026-11-18 — corpus composition, minimal-pair contrast rows,
register breadth and slot-cue selection. [`closed-lines.md`](closed-lines.md)
holds the measurements, the sets they were taken on, and the ratio a replacement
has to beat. An entry here is a candidate only if it clears that ratio.

## What already exists

Additions only make sense against a true baseline. Today the project has:

| Surface | Already built |
| --- | --- |
| Model | 5.25M-parameter int8 decoder, 6 layers, d=256, 640-entry BPE (208 merges), greedy decode, 20-token call budget |
| Contract | 171 fixed-arity relations, `CALL <name> <args>`, every argument must appear in the question |
| Solvers | Integer and exact-rational C for all 171 relations across eleven per-subject units, four-significant-digit rendering |
| Parser | Q16 expression parser for graphs: `+ - * / ^`, `sin cos tan sqrt abs ln exp`, `pi` and `e`, unary minus, decimals, `x`, 48 nodes; symbolic differentiation and numeric integration on the same tree, and exact rational `det`/`inv`/`rank`/`solve` |
| Graphing | Autoscale, pan, zoom, trace, root and extremum detection on a 320x240 framebuffer |
| Evaluation | Hand-written wording 74.0% exact, 15.3% right relation with wrong arguments, 10.7% wrong relation and 1 refusal on the 2,150-row calibrated eval split, seed 1 of the r5 pair; 70.9% exact as the r7 two-seed mean on the same 2,150 rows. Generated gate 89.8% exact on 2,000 held-out generated rows, r7 two-seed mean. C/PyTorch logit parity, ARM cross-compile check |
| Refusals | Three result codes the screen tells apart — arithmetic past the exact engine's node pool, arithmetic with no value, weights whose vocabulary disagrees with the build's — plus the decode's own uncertainty at a 500-unit relation margin. Each has a host test that asserts on the framebuffer |
| Platform | `.tns` build, weight file loading, a screen model in `runtime/ui.c` that replaces the old message boxes, and BPE and pipeline shared with host tests. None of it has run on a physical calculator |

The catalogue below assumes all of that stays.

### What the 2026-11-18 work added to that baseline

Nine things this page would have listed as candidates are built. Each line
names the check that carries it and the set its number came from.

| Change | What a student can now do | Evidence |
| --- | --- | --- |
| Arithmetic past the exact engine's node pool refuses as such | A column of arithmetic is told it is too long instead of being read as a word problem — a 65-term sum used to come back as `CALL h_from_ph 115 110`. A flat sum answers at 64 terms and 127 characters and refuses at 65 and 129 | `PIPELINE_TOO_LONG`; 0 of the 2,150 hand-written rows is typed arithmetic, so the limit costs that split nothing |
| Arithmetic with no value refuses as such | `1/0`, `0/0`, `sqrt(-4)`, `ln(0)`, `2^15`, `9^999`, `exp(100)` and `100000/3` — which answered `10923` — get one honest sentence instead of `32768` | `PIPELINE_NO_VALUE`: 14 cases refuse, 11 still answer exactly, 0 of the 4,300 hand-written rows (2,150 × 2 seeds) changed |
| Weights that disagree with the build's vocabulary refuse | The screen names both sizes instead of blaming the question | `vocab` is a consequence of the tokenizer, so a disagreement proves a mismatch; an agreement proves nothing, so no branch claims a match |
| Three relation names are typable at last | `mass_from_percent_composition`, `mixture_final_temperature` and `wavelength_from_frequency` resolve when typed. The 24-character buffer used to miss them and the decoder answered something else: `wavelength_from_frequency 200` came back `1 J/K` from `entropy_change` | `TOKLEN` 24 → 32, and `test_shorthand` now types every name in `reltable.h` |
| Slot prompts say what unit the slot wants | `v0 (m/s)` instead of `v0`, so a half-remembered formula can be filled correctly without the model | `runtime/unitstable.h`, 124 rows, ~3.7 KB of flash by arithmetic and not measured; each argument's unit is the majority answer across the phrasing bank |
| Find a relation, see its formula, reuse the last answer | Type three letters, pick the name, read the formula above the slots, and fill a slot with the previous answer on `TAB` — no model, so it cannot be wrong | `test_ui` asserts a framebuffer band against the frame drawn from the expected text |
| A growth rate is taken as written | 2.5%, 3.5% and 7.5% answer where only whole percent rates did | 26 corpus calls that refused now answer, 0 previously-answered corpus calls changed, and the 2,150-row hand-written split reads 1,577 → 1,588 (seed 1) and 1,560 → 1,568 (seed 2). One row moved from refusal to a wrong number and stays on the record |
| Three solvers stop printing a wrong number past the fixed point | `critical_angle 150 100` answered 2.968° where the angle is 41.81°, `law_cosines` with sides of 2000 answered 3587 where it is 5596, and `continuous_growth 1000 10 40` answered 4455 where it is 5.46e4 — all reachable from typed input, none from the corpus | 0 of the 4,571 distinct corpus calls changed answer; the sweep that found them compared answered values, not just counted answers |
| A call the decode barely chose is refused | Answering "uncertain" instead of a confident number built on the runner-up relation | 22 rows caught and 7 lost over the 4,300 hand-written rows, 3.1:1 at 76% precision; the 2,000-row generated control is 1 and 1 and never exceeds 2:1 at any threshold. Exactness does not rise: 73.1% → 72.9% |

What did **not** become true is worth the same sentence: on the real AP Chemistry
bank the app names a correct call for 1 of 19 readable rows, the bank's own shape
caps trustworthy single-call rows at 28, and neither is a wording problem. See
[`status.md`](status.md) for the limit and [`closed-lines.md`](closed-lines.md)
for the four lines that were measured and closed on the way to it.

## A. Model and inference

| # | Capability | What changes | Model | Work |
| --- | --- | --- | --- | --- |
| A1 | Abstention | An `unsupported` outcome from the call-token margin, plus out-of-scope training rows. The margin half is built (#122) at 500 fixed-point units: 22 caught and 7 lost over 4,300 hand-written rows, and a generated-gate control of 1 and 1. The out-of-scope rows are not built | none for the margin; retrain for the rows | days |
| A2 | Constrained decoding | Trie over relation, arity and numeric grammar so an invalid call cannot be emitted. The numeric-argument half was built and reverted: exact calls identical, none of 253 re-decoded refusals became exact — see [`closed-lines.md`](closed-lines.md) | retrain | days |
| A3 | Multi-step tape | `@r1` argument references, a call tape, and composition training rows. This is the entry the AP Chemistry bank argues for and the only one that moves its ceiling | retrain | weeks |
| A4 | Longer calls | Raise the 20-token generation cap and the 192-token training window. The typed input line is 255 characters (#118) and arithmetic past the exact engine's 127 characters now refuses by name rather than reaching the decoder; 9 of the 28 AP Chemistry rows still exceed the window | retrain | days |
| A5 | Wider model | d=352 (9.6M) or d=512; measured at 3.0 tok/s vs 5.5 on the same hardware | retrain | weeks |
| A6 | Domain tokenizer | More merges, subject tokens, reserved tokens for units and `@` | retrain | days |
| A7 | Ranked alternatives | Top-3 candidate calls with margins, so a near miss is visible to the user | retrain | days |
| A8 | Better quantization | Group 16 or mixed precision on attention only; measure logit drift | retrain | days |
| A9 | Real typed wording | Hand-written and paraphrased questions in training, not just the phrasing bank. Built: three rounds are in the corpus and a fourth is parked on a branch, and rounds three and four flattened — the last 2,655 rows restored by fixing the quantiser bought +0.45 points against a 2.6-point seed spread. More rows of the same kind are not the lever | done for the corpus; retrain for anything more | weeks |
| A10 | Adversarial distractors | Two or more same-unit quantities, values that belong to a different relation. Attempted in round four: burying the real rows among distractors moved exact calls 66.7% → 68.0% on the same 2,512 rows, a 1.3-point gain at a 0.9-point standard error. Not the lever either | retrain | days |
| A11 | Curriculum | Order training by relation difficulty; trim the 200k set to the rows that teach something | retrain | days |
| A12 | Edit-distance recovery | A character-level repair pass when grounding fails, for a mistyped digit | none | days |

A5 is listed for completeness and is the one entry the evidence argues
against: the repository's own width ladder shows shrinking costs 2.5 points on
grounded computation and 69 points on explanation, which means routing survives
small models.

**A3 is now the ceiling, and the AP Chemistry bank is what proves it.** The bank
was audited item by item: of the 70 multiple-choice rows that survive a
mechanical gate, a hand read finds 28 answerable in one call, 23 that need a
second computation, 6 whose missing argument is a tabulated constant, 9 where a
cue word matched but the ask is a different quantity, and 3 with no relation at
all. Bank-wide, 835 of 1,136 items name no catalogue relation and 51 name two or
more. The limit on "accurate on AP Chemistry" is two- and three-step questions,
not wording, and A3 is the entry that would move it.

## B. Mathematics core

| # | Capability | What changes | Model | Work |
| --- | --- | --- | --- | --- |
| B1 | Units as a type | Dimension exponents and an affine-offset flag on every quantity; refuse on mismatch. The assignment half is measured down to 1.3 points — see below | none | weeks |
| B2 | Direction-agnostic equations | One equation plus `solve_for(unknown)` instead of a relation per direction | retrain | weeks |
| B3 | Expression parser beyond graphing | `ln exp sqrt abs floor`, `pi`/`e`, multi-letter variables, general `a^b` | none | weeks |
| B4 | Symbolic differentiation | `d/dx` on the parse tree, then evaluate at a point or print the result | none | weeks |
| B5 | Numeric integration | Adaptive Simpson, plus trapezoid for tabulated data | none | days |
| B6 | Numeric root finding | Bisection and Newton on the parse tree, beyond the graph's sampled roots | none | days |
| B7 | n×n linear solve | Gaussian elimination with partial pivoting over exact rationals | none | days |
| B8 | Eigenvalues | 2x2 and 3x3 closed form; characteristic polynomial for the rest | none | days |
| B9 | ODE integration | RK4 for cooling, kinetics, RC circuits, population models | none | days |
| B10 | Multi-value outputs | Solver returns a vector (roots, angles, systems) rather than one string | retrain | days |
| B11 | Complex numbers | Rectangular and polar arithmetic, complex roots of quadratics | none | days |
| B12 | Property tables | Compact compressed saturated-water, cp, and standard-enthalpy tables | none | weeks |
| B13 | Statistics | Mean, median, variance, regression, normal and t quantiles, chi-square | none | days |
| B14 | Probability | Permutations, combinations, binomial and Poisson mass functions | none | days |
| B15 | Matrices and vectors | Determinant, inverse, dot and cross product, projections | none | days |
| B16 | Number theory | GCD, modular exponentiation, primality, base conversion | none | days |
| B17 | Symbolic integration | Table-based plus substitution and integration by parts | none | months |
| B18 | Full CAS | Simplification, assumptions, series, limits | none | years |

B1 through B3 are the load-bearing entries. Everything from hard thermodynamics
to arbitrary calculus input depends on them, and none of them is a model change.

**B1's assignment half, measured 2026-11-17 — and it is not worth building.** Of
139 hand-written rows that picked the right relation with the arguments permuted,
a dimension-only assigner decides 56 (40%), and every one of those 56 lands
exactly on the expected call. Precision is perfect; reach is the whole limit, so
the recoverable accuracy is **1.3 points of hand-written exactness and no more**,
not the 3.2 a permutation count suggests. The residue is not bookkeeping: 48 rows
write an argument with no unit at all, 25 leave two or more dimension-compatible
assignments, and 10 write a unit that contradicts the slot. Adding quantity nouns
as a fallback cue buys three rows, taking it to 1.37, which is why the ceiling is
quoted as 1.3.

Revisit it only with a corpus that writes units on nearly every argument, because
that is what would move the 48 no-unit rows into reach. The artifact at
`handoffs/ndle21-dimslots/` replays the number rather than re-arguing it:
`python3 dimslots.py --rows permuted_rows.jsonl` prints 139 rows, 56 decided and
the reason for each of the rest, and `python3 nouncues.py --rows
permuted_rows.jsonl` prints the noun-cue comparison.

**B1's unit half is built and measured, and it is not merged.** A contributor's
five-pull stack gives every one of the 171 relations a slot unit and converts a
number stated in another unit of the right kind before the solver runs, and
refuses a number of the wrong kind. Measured on the stack rebased onto master
`259021ee`: **0 of 48,672 correct corpus calls changed**, 147,842 single-unit and
22,285 same-unit-group conversions answered exactly what the original answered
with 0 refused, and **36,509 of 107,923 swapped-argument calls (33.8%) were
refused**. Their own figures at their base were 45,808 calls and 139,885 + 20,925
conversions, 0 changed, so the result survives master's growth. Three measured
blockers keep it off master — a pipeline code that collides with
`PIPELINE_TOO_LONG`, one stale `carnot` expectation, and a stack layer based on a
branch with no pull request.

## C. Subject breadth

Relations are cheap once B1 and B2 exist, because a relation becomes a formula
and a dimension signature rather than a hand-written C function.

| # | Subject | Relations worth adding |
| --- | --- | --- |
| C1 | Thermodynamics | calorimetry, latent heat, first law, pΔV work, isothermal, adiabatic, Carnot, COP, entropy, Gibbs, conduction, Stefan-Boltzmann, `v_rms`, Newton cooling |
| C2 | Mechanics | 2D kinematics, projectile x and y separately, collisions, energy conservation, torque, moments, rotational KE, friction, springs |
| C3 | Waves and optics | superposition, standing waves, beats, Doppler, Snell, thin lens, mirrors, diffraction |
| C4 | Electromagnetism | series and parallel networks, capacitors, RC time constant, inductors, magnetic force, Faraday, Coulomb, electric field |
| C5 | Modern physics | photoelectric, de Broglie, energy levels, half-life chains, binding energy, relativistic factors |
| C6 | Calculus | product, quotient and chain rules, higher derivatives, partial derivatives, series convergence, arc length, surface of revolution |
| C7 | Linear algebra | systems in 3 and 4 unknowns, matrix products, rank, least squares |
| C8 | Chemistry | balancing equations, limiting reagent, equilibrium, buffers, titration, rate laws, cell potentials |
| C9 | Discrete and CS | sets, relations, graph traversal, recurrence solving, big-O comparison, truth tables |
| C10 | Finance and economics | NPV, IRR, annuity, depreciation, elasticity, break-even |
| C11 | Statistics | confidence intervals, hypothesis tests, regression diagnostics |
| C12 | Units and constants | full SI conversion graph, dimensional analysis, physical constant table |

C8 needs a formula parser before it is worth starting; balancing equations is
linear algebra over integer stoichiometry, not a numeric call. That judgement was
tested: `balance_coefficients` (an equation in, a coefficient vector out),
`electrons_transferred` (needs the balanced equation and oxidation states) and
`oxidation_number` (a species in, a state out) were all attempted against the AP
Chemistry bank and cut as unbuildable in a catalogue where every call has a
number for an answer, which is why the eighteen-relation chemistry batch landed
without them.

## D. Interaction and interface

| # | Capability | Notes |
| --- | --- | --- |
| D1 | Multi-function plotting | Several curves with distinct colours and a legend |
| D2 | Graph overlays | Shade the definite integral, draw the tangent, mark roots and extrema |
| D3 | Table view | `x` and `f(x)` columns, scrollable |
| D4 | Inequality shading | Parse `< <= > >=` and fill the region |
| D5 | Step-by-step display | Show the call, the formula, the substitution, the result — no model involvement |
| D6 | History | Scrollable log of questions and answers, persisted to a file |
| D7 | Answer reuse | Send the last answer into the next question as a slot value — **built** (#104): `TAB` in a slot fills it with the previous answer, so `kinematic_v 0 3 4` → 12 m/s is available to `kinetic_energy` as 12 |
| D8 | Template keypad | Freeform text input is the worst part of the current UI; a keypad grid of relation templates removes it — **built in two forms** (#103, #104): find a relation by three letters of its name from the menu, and its formula printed above one prompt per argument |
| D9 | Practice mode | Generated problems by subject and difficulty, with marking |
| D10 | Spaced repetition | Track per-relation accuracy per user and re-ask the weak ones |
| D11 | Unit prompt | When units are ambiguous, ask which unit the answer should use — **built in the half that matters** (#153): each slot prompt states the unit the phrasing bank uses, `v0 (m/s)`, so the unit is named before a number is typed rather than queried after |
| D12 | Error explanations | "I matched this to Ohm's law, but 12 does not appear in the question" — **built for the three pipeline failures** (#138, #144, #146) and for a weights vocabulary that disagrees with the build; the old screen blamed the tokenizer on every refusal, which was wrong advice |
| D13 | Settings | Screen contrast, significant figures, exact vs decimal, default units |
| D14 | Localisation | Language packs for phrasing banks and UI strings |
| D15 | Large-text mode | Readable on the calculator's low-resolution screen |
| D16 | Export | Write a results file, or a problem set the teacher supplied |

D5, D8 and D12 are the three highest-value interface entries: the answer is
already correct in most cases, and the interface is what makes it believed.

Wave 4.5 shipped four of these in the host-testable form: D5 (the answer pane
prints the call, the formula with the numbers substituted, and the result), D6
as RAM only — eight pairs of question and answer with one-key recall, lost when
the app exits, because persistence needs a writable file on a filesystem this
project has not yet touched — D8 as four scrollable pages, three of typed syntax
and one of typed calls, rather than a keypad grid, and D12 for the three
pipeline failures. D2 and D3 are the graph overlays and the table view: the
shaded integral and the marked answer exist in 4.6, the scrollable table does
not.

D8 landed in wave 5 in two pieces, neither of which needs the model: typed calls
(`fri 0.4 500`) and solve by formula (subject, then relation, then one prompt per
argument, named from the relation's own `params`). See `wave5.md`. What is still
missing is D8's last form, a phrasing with its numbers blanked.

## E. Platform and Ndless

| # | Capability | Notes |
| --- | --- | --- |
| E1 | OS hook | Hook TI's own input or home-screen answer so the solver is reachable without launching an app |
| E2 | File association | `cfg_register_fileext` for a problem-set extension |
| E3 | USB serial protocol | Serve questions over USB for a classroom or a host harness |
| E4 | Problem-set import | Read a text file of questions and write answers beside them |
| E5 | Persisted config | `cfg_get`/`cfg_open` for settings and history |
| E6 | Single-file packaging | Embed or append weights to the `.tns` so two files cannot be mismatched |
| E7 | Flash streaming | Load layers on demand instead of holding all weights, freeing heap |
| E8 | CPU speed control | `set_cpu_speed` around inference, restore after |
| E9 | Lua binding | Expose solvers to `luaext` scripts |
| E10 | SDL frontend | Reuse the existing SDL libraries for a richer UI |
| E11 | Emulator harness | Scripted end-to-end run in an Nspire emulator as part of CI |
| E12 | Multi-model dispatch | Ship a small model for routing and a larger one loaded only when asked |

E6 is worth doing early and cheaply: the stale-weights failure mode is the most
expensive debugging trap in the repository today.

## F. Data, evaluation and tooling

| # | Capability | Notes |
| --- | --- | --- |
| F1 | Real wording in the gate | Promote `stress_cases.py` from a script to a gate; it is the largest unmeasured generalisation risk |
| F2 | Generated C solvers | Emit the C from the relation spec instead of hand-porting 86 lines per relation |
| F3 | Property-based parity | Random inputs through both implementations, not just generated fixtures |
| F4 | Per-relation dashboard | Exact and wrong rates by relation, tracked across commits |
| F5 | Calibration reporting | Does a model margin of 0.9 mean the answer is right? |
| F6 | Latency harness | End-to-end timing on real hardware, not a cycle model |
| F7 | Deterministic datasets | Byte-reproducible generation across machines and library versions |
| F8 | CI | Run the gates on every push, with the accuracy gates on a cached checkpoint |
| F9 | Quantization ablation | Group size and bit-width sweep against the parity fixture |
| F10 | Model card automation | Regenerate the measured tables from the last run |

## G. Product and distribution

| # | Capability | Notes |
| --- | --- | --- |
| G1 | Installer integration | Bundle the app with the Ndless install flow |
| G2 | Release artifacts | Published `.tns` plus weights, with a checksum |
| G3 | On-device diagnostics | Show saturation counts and the decoded call when something fails |
| G4 | Documentation for users | Beyond the developer docs, which are already good |
| G5 | Curriculum alignment | Map relations to a syllabus so coverage gaps are visible |

## Budgets every entry must respect

Held-out accuracy is not the only gate. Measured constraints:

| Quantity | Value |
| --- | --- |
| Weights re-read per token | 5.25 MB at 70.5 MB/s, a 77 ms floor |
| Decode speed | 5.5 tok/s at 144 MHz, 7.6 tok/s at 198 MHz |
| One call | 7.4 tokens, about 1.4 s |
| A three-call chain | about 4 s, still tolerable |
| Largest single allocation | 21.56 MiB of a 27.12 MiB heap |
| Wrong answers | worse than refusals, at every tier |

Anything that pushes a single answer past roughly five seconds, or that needs
the whole model resident twice, is out of budget rather than merely expensive.

## Deliberately not planned

| Not doing | Why |
| --- | --- |
| General reasoning or tutoring | 20M parameters at 198 MHz is seconds per token with no capacity for open-ended work |
| Reaching TI's CAS | Ndless apps cannot call it |
| A larger model as the answer to capability | The model routes; capability lives in solvers |
| Floating point in the hot path | No FPU, and soft-float helpers fail the cross-compile check |
| On-device training | Out of budget and out of scope |
| Dimension-based argument assignment | Measured at 1.3 points of hand-written exactness on the current corpus: 56 of 139 permuted rows, all decided correctly, and noun cues buy three more. Reach, not precision, is the wall — see B1 |
| Constraining decoded arguments to the question's numbers | Exact calls unchanged on both seeds (1,577 and 1,560 of 2,150), and none of 253 re-decoded refusals became exact. A mask reaches which numbers are available; the error is a choice among the ones already there — see [`closed-lines.md`](closed-lines.md) |
| Corpus composition as an accuracy lever | Fixing the hand-written share restored 2,655 training rows and bought **+0.45 points** on the 2,150-row hand-written split against a **2.6-point** seed spread — [`closed-lines.md`](closed-lines.md) |
| Minimal-pair contrast rows | Flat on the 333 held-out rows covering the same eight pairs: 81.1% (r6 mean) → 80.95% (r7) — [`closed-lines.md`](closed-lines.md) |
| Register breadth as an accuracy lever | The drift cohort moved 41.6% → 47.6% on held-out structures while every accuracy measurement stayed flat — [`closed-lines.md`](closed-lines.md) |
| Slot-cue selection of argument values | A good predictor (81.9% of argument vectors against 66.1% for reading order) and a bad change: **74.0% → 70.5%, −77 rows**, because it fixes 55% of errors and breaks 16% of correct calls and correct calls outnumber errors five to one — [`closed-lines.md`](closed-lines.md) |
