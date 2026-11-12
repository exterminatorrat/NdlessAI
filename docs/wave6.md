# Wave 6: capability, not just formulas

The current program answers one question by picking one relation and evaluating
it. A CAS does the arithmetic and algebra of a formula *the student already
wrote*; this program's whole claim is that it reads the words. So the thing to
build next is not "more formulas a CAS could also do" — it is the part of a hard
problem that happens before the arithmetic: deciding what the question is,
which models apply, in what order, in what units, and whether the result is
possible.

**The target is a question a CAS cannot answer without being told how.** "A 2 kg
block slides 4 m down a 30° ramp with μ = 0.2 and then across a rough floor with
μ = 0.5; how far does it travel before stopping?" A CAS will happily solve any
one formula in that chain, and be useless, because the work is the chain. That
sentence needs: resolve gravity along the ramp, subtract friction over the ramp,
convert to speed at the bottom, dissipate that energy over the floor, and hand
back a distance — five relations, one unit discipline, and a decision about
which energy to carry.

So the wave divides in two: **wording** (read anything a teacher would write) and
**composition** (solve things that take more than one step), with **trust** as
the thing that makes either usable on a calculator a student bets marks on.

## Where the program is today

The baseline the plan has to move, measured rather than remembered:

| | Now |
| --- | --- |
| Relations | 136, one call each, in the tokenizer's own table |
| Model | 256-wide, 6 layers, 8 heads, 768 hidden, int8, 512-token vocabulary |
| Sequence | trained at 160 tokens; the mean question is 60 |
| Accuracy | 90.3% exact call on generated held-out, 71.8% on hand-written paraphrases |
| Ambiguity | 50% of training rows carry a distractor number the answer must ignore |
| Structure | **one** `CALL` per question. There is no way to express two steps |
| Units | carried as text of the answer, never parsed or checked |
| Verification | arguments must appear in the question; nothing checks the physics |

Everything below is measured against that. `R` marks features that need a
retrain; an empty mark means code and data only.

## A. Wording: read anything a teacher would write

The gap is real — 71.8% on hand-written phrasing against 90.3% on generated —
and it is the same gap every round of new wording data has failed to close,
because the model is being asked to map a sentence to an id with no help.

| # | Feature | Why it matters | Cost | R |
| --- | --- | --- | --- | --- |
| W1 | Per-relation wording variance as a gate | Averages hide that `sig_figs` and a few others carry the whole deficit; nothing fails a build when one relation's phrasing collapses | S | |
| W2 | Slot lexicon with synonyms | "push / force / pull / thrust" and "speed / velocity / pace" map to one slot; today each is a separate thing to memorise from data | M | ✓ |
| W3 | Typos and truncation tolerance | Keypad typing produces `accelartion`; a spell-distance match on the slot vocabulary is cheaper than training every misspelling | M | |
| W4 | Number-as-word parsing | "twelve", "a dozen", "half", "one and a half" — a student writes these, and the tokenizer sees letters where the solver needs a number | S | |
| W5 | Units in the text as first-class values | `45 km/h`, `3.2e5 Pa`, `0.45 kg` should arrive as quantity, not as digits plus noise | M | |
| W6 | Engineering prefixes | "thirty milliamps", `4.7k`, `2 MΩ` — the calculator's own keypad has an EE key for exactly this | S | |
| W7 | Polarity and direction words | "decreases by", "slower", "opposite", "leftwards" decide signs; today a sign flip is indistinguishable from a right answer | M | ✓ |
| W8 | Comparatives and ratios | "how many times larger", "what fraction of" — the answer is a ratio of two computed quantities, so this is also an S-item | S | |
| W9 | Conditional questions | "if μ doubles, what happens to f?" — needs two solves and a comparison, and it is the format that tests understanding | M | |
| W10 | Multi-clause sentences | The ramp-plus-floor sentence above is three clauses and one question | L | ✓ |
| W11 | Implicit givens | "a 5 kg block on a table" implies the normal force and weight; today every number must appear explicitly | M | |
| W12 | Default constants with stated assumption | g = 9.8 vs 9.81 vs 10 changes the third digit; the answer should say which it used | S | |
| W13 | Ambiguity → question, not guess | When two relations are within a margin, ask "did you mean X or Y?" — the runtime already computes the top three with their margins | S | |
| W14 | British and American terms | "moment" vs "torque", "weight" vs "mass" in student speech, "litre" / "liter" | S | |
| W15 | Abbreviations | `vel`, `acc`, `temp`, `conc`, `molar`, `keq` — exam shorthand | S | |
| W16 | Reference resolution | "it", "the same block", "that value" across the sentence and into the previous answer | M | |
| W17 | Multi-part questions | "find the speed and the kinetic energy" — two asks, one sentence | M | ✓ |
| W18 | Exam register | The difference between "A car accelerates uniformly from rest…" and "car goes vroom, how fast" is register, not content | M | ✓ |
| W19 | Long-context questions | 178 tokens is the current maximum; real exam items with context run past the 160-token window | L | ✓ |
| W20 | Distractor hardening | 50% of rows already have an unused number; measure the failure as "which number did it pick" per relation | M | |
| W21 | Number order independence | The same values in a different order must not change the call — the model currently reads left to right | M | ✓ |
| W22 | Implicit operations | "the difference between", "how much more", "by what factor" | S | |
| W23 | Ask for a formula, not a number | "what equation do I need?" answered from the relation table — no model at all | S | |
| W24 | Explain and show-steps commands | "why", "show working" — the step templates already exist for 123 relations | S | |
| W25 | Spelling of units in words | "newtons", "joules per kilogram", "metres per second squared" | S | |
| W26 | Question templates from the bank | The phrasing data already exists; expose a page of them as fill-in-the-blank prompts so a student can practise the wording the model knows | M | |
| W27 | Non-English input | Spanish and Chinese phrasing banks — the same relation ids, a second surface | L | ✓ |
| W28 | Per-phrasing provenance | When a paraphrase fails, name the phrasing family so the next data round targets it instead of guessing | S | |

## B. Composition: problems that take more than one step

This is the part a CAS does not have, and the reason someone would use this
instead of one. It needs one new idea in the protocol — a question may answer
with a *plan*, not a call — and the runtime already has everything to execute
one.

| # | Feature | Why it matters | Cost | R |
| --- | --- | --- | --- | --- |
| S1 | Multi-call plans | `CALL a ... ; CALL b ... <1>` — a result feeding the next step. The single change that makes 136 one-step relations into a solver | L | ✓ |
| S2 | Plan length up to five | A ramp-floor problem needs four; a titration five; more than five is a different product | M | ✓ |
| S3 | Named intermediate results | "call it F" / "<1>" placeholders, so the plan is readable as working, not just executable | S | |
| S4 | Substitution into a later call's arguments | The runtime already renders steps with numbers substituted; a plan needs the same machinery to feed values | M | |
| S5 | Plan verification before execution | Each step's arity and units checked before the first result is computed, so a bad plan fails loudly instead of mid-way | M | |
| S6 | Inverse solving from words | "what mass gives 12 N at 4 m/s²" — solve for any argument, which the equation engine already does for typed input | M | |
| S7 | Systems of two or three equations from text | Two unknowns, two sentences: a classic exam shape and a natural plan | L | ✓ |
| S8 | Constraint problems | "at least", "no more than" produce inequalities, not equalities | M | |
| S9 | Dimensional analysis as a checker | Every relation gets declared dimensions; a plan whose units do not cancel is refused before it prints | L | |
| S10 | Multi-stage motion | Accelerate, coast, brake — three kinematics calls chained | M | ✓ |
| S11 | Conservation-law choice | Energy vs momentum vs kinematics for the same words; the plan states which and why | L | ✓ |
| S12 | Circuit networks | Series/parallel reduction and Kirchhoff, from a text description | L | ✓ |
| S13 | Multi-step stoichiometry | Limiting reagent, then yield, then concentration | L | ✓ |
| S14 | Equilibrium with a quadratic | ICE tables, Kc/Ksp, small-x approximation stated as an assumption | L | |
| S15 | Titration regions | Strong/weak, buffer, equivalence, excess — four formulas in sequence | L | ✓ |
| S16 | Cycles and efficiencies | Carnot, then work, then power, chained | M | |
| S17 | Related rates | Two quantities, one derivative, one chain rule | M | |
| S18 | Optimization with a constraint | Objective plus constraint, then a derivative and a solve | L | ✓ |
| S19 | Vector composition | Components, then magnitude and direction, then a second law | M | |
| S20 | Statics | Sum of forces and torques both zero, solved together | L | |
| S21 | Probability word problems | Counting, conditional, expected value | L | |
| S22 | Statistics from a list | Mean, spread, line of best fit, prediction | M | |
| S23 | Finance schedules | Annuity, amortization, NPV — a plan over a table | M | |
| S24 | Projectile with drag | Numeric integration (RK4 exists) chosen because no closed form applies | M | |
| S25 | Non-separable ODEs | Numeric solution with a stated step size, which is already implemented | S | |
| S26 | Piecewise and threshold problems | "until it stops", "once it reaches" — a branch in the plan | L | ✓ |
| S27 | Implicit-unit handoff | A step's output unit is the next step's input unit, converted rather than assumed | M | |
| S28 | Sanity check on every plan result | Orders of magnitude and sign against the declared dimensions | M | |
| S29 | Two independent routes | Kinematics and energy both solve the same problem; agreement is evidence, disagreement is a refusal | L | ✓ |
| S30 | Worked-solution output | The plan printed as the solution, each step with its formula and numbers, which is exactly what the step templates already render | M | |
| S31 | Ask-back when a step is underdetermined | "I need the coefficient of friction — do you have it?" instead of refusing flatly | M | |
| S32 | Units in the prompt | The picker's argument prompts become unit-aware ("mu", "N (newtons)", "theta (degrees)") | S | |
| S33 | Reusable plan library | A dozen named strategies (ramp, projectile, RC, titration) that the planner prefers over novel compositions | M | |
| S34 | Plan-level refusal | "This needs the time and you gave the distance; I cannot get one from the other here" | M | |

## C. Trust: the answer has to be worth believing

The answer is already right most of the time; what fails is the student's
ability to tell which times. Every item here is about the second number on
screen, not the first.

| # | Feature | Why it matters | Cost | R |
| --- | --- | --- | --- | --- |
| T1 | Confidence per answer | The decoder's margin already exists (top-3 kept by 4.6c); show it as a word, not a fabricated percentage | S | |
| T2 | "Why this relation" | Name the two runners-up and the one number that decided it | S | |
| T3 | Dimensional check on the result | A friction force arriving in joules is a wrong answer that looks right | L | |
| T4 | Plausibility screen | 10⁹ N for a block on a table should be refused, not printed | M | |
| T5 | Span-grounded arguments | Every argument must map to a span of the question, not just a value that appears somewhere | M | |
| T6 | Permutation self-check | Re-ask with the numbers reordered; if the call changes, lower the confidence | M | |
| T7 | Second route on demand | "check" runs the independent route (S29) and reports agreement | L | |
| T8 | Assumption listing | "assumed g = 9.8, no air resistance, uniform friction" under the answer | S | |
| T9 | Unit prompt | When the answer's unit is ambiguous, ask which the student wants | S | |
| T10 | Significant figures policy | Per subject and per exam board, and a toggle between exact and decimal | M | |
| T11 | Refusal taxonomy | Four named failures with different next steps: no model, unreadable, missing datum, ambiguous relation | S | |
| T12 | Answer provenance line | Which relation, which numbers, which assumption, in one line — the CALL line already half does this | S | |
| T13 | Calibration measurement | Bin confidence against correctness on the held-out set, so "high confidence" means something | M | |
| T14 | Failure log on the device | Keep discarded questions in a ring buffer so the next data round has real material | M | |

## D. Knowledge breadth: what the model can be asked about

Breadth is the cheapest capability per unit of work — a relation is 14 lines of
Python, 27 of C, and a parity vector — but it is only worth doing where the exam
syllabus actually asks. Grouped by what a student would hit.

| # | Feature | Why it matters | Cost | R |
| --- | --- | --- | --- | --- |
| K1 | Rotation (AP Physics C) | Moments of inertia, angular momentum, rolling, torque as a vector | M | ✓ |
| K2 | Gravitation | Orbits, escape velocity, potential energy wells | S | ✓ |
| K3 | Oscillations | SHM period, damping, resonance | S | ✓ |
| K4 | Electromagnetic induction | Flux, Faraday, Lenz, motional emf | M | ✓ |
| K5 | Capacitor and inductor transients | RC and RL with a time constant — a two-step plan | M | ✓ |
| K6 | Circuit reduction | Series/parallel networks with a stated topology | M | ✓ |
| K7 | Optics beyond Snell | Thin films, gratings, polarization, lenses combined | M | ✓ |
| K8 | Standing waves and beats | Nodes, harmonics on strings and pipes | S | ✓ |
| K9 | Doppler in two dimensions | Moving source and observer | S | ✓ |
| K10 | Entropy and the second law | ΔS for the textbook processes | M | ✓ |
| K11 | Real heat engines | Otto, Diesel, Brayton efficiencies | M | ✓ |
| K12 | Kinetic theory | RMS speed, mean free path (numerically feasible now) | S | ✓ |
| K13 | Viscosity and Poiseuille | Laminar flow, terminal velocity | S | ✓ |
| K14 | Bernoulli with losses | Head loss, pumps | M | |
| K15 | Nuclear decay chains | Activity through two decays, binding energy from a mass table | M | ✓ |
| K16 | Photoelectric detail | Retarding potential, threshold graphs | S | ✓ |
| K17 | Chemistry equilibria | Ksp, common-ion, buffer capacity | M | ✓ |
| K18 | Kinetics orders | Rate law from data, half-life per order | M | ✓ |
| K19 | Electrochemistry | Nernst, cell potential, electrolysis mass | M | ✓ |
| K20 | Organic stoichiometry | Combustion, empirical formula, percent yield | M | ✓ |
| K21 | Integration techniques | By parts, partial fractions, trig substitution, improper integrals | L | |
| K22 | Volumes and arc length | Solids of revolution, surface area, curve length | M | |
| K23 | Polar and parametric areas | A BC topic the graph engine can already draw | M | |
| K24 | Series convergence with limits | The gap 4.2 left; needs a limit engine, which is the honest blocker | L | |
| K25 | Power series operations | Multiply, differentiate, integrate a series | M | |
| K26 | Linear algebra beyond 3×3 | Elimination-based determinant, rank, nullspace, least squares | L | |
| K27 | Eigenvalues | 2×2 analytic, 3×3 numeric | M | |
| K28 | Second-order ODEs | Characteristic roots, forced response, systems | L | |
| K29 | Probability distributions | Binomial, normal CDF and inverse | M | |
| K30 | Complex numbers | Arithmetic, polar form, roots of unity | M | |
| K31 | Discrete maths | Recurrences, big-O, truth tables, set operations | M | |
| K32 | Physical constants as first-class | Query `c`, `h`, `N_A`, `k_B` and use them in expressions | S | |
| K33 | Formula-sheet browser | Read the catalogue by subject, with units, with no model needed — it exists already as data | S | |
| K34 | Unit systems | SI and US customary, with conversion chains and sig figs | M | |

## E. Model and architecture: how to afford all of it

The model is 256-wide and already near its ceiling at 136 relations
(hand-written accuracy has not moved across 84, 90, 102 and 136). Breadth alone
will not fix that, so the architecture has to change shape rather than grow.

| # | Feature | Why it matters | Cost | R |
| --- | --- | --- | --- | --- |
| M1 | Decouple "which relation" from "which numbers" | Two small problems instead of one large one; the classifier is 136-way, the extractor is per-slot | L | ✓ |
| M2 | Retrieval over a formula index | Rank relations by embedding similarity instead of a 136-class softmax, so adding the 200th relation costs nothing | L | ✓ |
| M3 | Grammar-constrained plan decoding | A plan DSL with a real grammar, decoded with a trie mask — the call mask already works this way | L | ✓ |
| M4 | Teacher-generated paraphrases, verified | An LLM writes phrasings, the Python parity harness checks them; the verifier is what makes synthetic data safe | M | |
| M5 | Distillation from a larger teacher | Train wide, ship narrow; accuracy of a bigger model at the latency of this one | L | ✓ |
| M6 | Wider model, measured on the device | The capacity decision wave 3 deferred — do it with a stopwatch, not a paper estimate | M | ✓ |
| M7 | Character-level numbers | Digit tokenisation is the weakest part of a 512-token vocabulary | L | ✓ |
| M8 | Speculative/cached decode | Every relation's prefix is shared; caching the first tokens is free latency | M | |
| M9 | Abstention training | Teach "I do not know" as a first-class output, then calibrate it (T13) | M | ✓ |
| M10 | Device failure ring buffer feeding the next round | Real questions are better data than generated ones (T14) | M | |
| M11 | Quantisation-aware training | The int8 path is the shipped path; train for it rather than around it | M | ✓ |
| M12 | Per-subject model heads | Physics and chemistry phrasing barely overlap; two small heads may beat one large | L | ✓ |
| M13 | Retrieval-augmented constants and units | A small on-device table beats asking the model to remember `c` | S | |
| M14 | Latency budget as a gate | Today only accuracy is gated; seconds-per-answer on the device is the number a student feels | M | |
| M15 | Regression suite of real exam items | 200 hand-written items from past papers, run every PR, reported per topic | M | |
| M16 | Error taxonomy mined from the suite | Every failure labelled by cause (wording, relation, number, unit, plan) so work is aimed | M | |

## F. Interface: a hard problem needs more than a text box

| # | Feature | Why it matters | Cost |
| --- | --- | --- | --- |
| U1 | Given/asked table | Before solving, show what was extracted and what is missing — the cheapest way to make a wrong read visible | M |
| U2 | Plan shown before the answer | "I will: energy down the ramp → friction on the floor" — lets the student stop a bad plan | M |
| U3 | Step reveal | Answer first, working underneath on a key, so the number is not buried | S |
| U4 | Diagram-first solving | Draw the ramp or the circuit, then ask; the diagram engine already draws six of these | L |
| U5 | Multi-part answer layout | Parts a/b/c with their own lines and a scroll position that remembers | M |
| U6 | Saved problems | Keep five worked examples on the device for revision | M |
| U7 | Practice mode | Generate variants of a solved problem and mark them | M |
| U8 | What-if sliders | Sweep one argument through the plan and watch the answer move — 4.6d already plots families | M |
| U9 | Undo and edit | Change one number in a solved question and re-run the plan, not the words | M |
| U10 | Table of values output | For a plan, show the intermediate values as a table | S |
| U11 | Unit chips on entry | After a number, offer the unit rather than requiring it typed | M |
| U12 | Exam timer/scratchpad | The calculator's real job during a test | M |

## Sequencing

Six slices, ordered by how much they unlock per unit of change, each with the
number that shows whether it worked.

| Slice | What ships | Unlocks | Measure |
| --- | --- | --- | --- |
| 1 | Plan protocol (S1–S5), one worked chain per subject | Every one-step relation becomes composable; the ramp-floor sentence becomes answerable | 50 hand-written two-to-four-step items, exact plan and final answer |
| 2 | Wording program (W1–W7, W20–W21) | Hand-written accuracy, which has been stuck at ~72% for four rounds | hand-written gate up 8 points, per-relation floor above 60% |
| 3 | Trust (T1–T5, T11–T13) | An answer a student can bet marks on, including the refusals | calibration error under 0.1, no wrong-unit answer |
| 4 | Dimensional layer (S9, S27, K32, K34) | Physics and chemistry chains that cannot silently mix units | zero dimensionally impossible answers in the regression suite |
| 5 | Breadth (K1–K20) | AP Physics C, AP Chem, and the BC calculus gaps | 60+ new relations, parity clean, no accuracy loss |
| 6 | Architecture (M1–M2, M5) | Room for the 300th relation without another capacity cliff | same accuracy with a 2× catalogue, no latency regression |

The order is not arbitrary: slice 1 makes the product different from a CAS,
slice 2 makes it work on real sentences, slice 3 makes it trustworthy, and only
then does breadth pay, because breadth on an unreliable base multiplies the
number of ways to be confidently wrong.

## Slice 1, as it stands

The protocol and the executor are in, and they work without a retrain because a
student can type a plan:

```
newton2 2 9.8 ; friction_force 0.2 <1>     ->  3.92 N
kinematic_v 0 3 4 ; kinetic_energy 5 <1>   ->  360 J
kinematic_v 0 2 5 ; kinetic_energy 2 <1> ; power <2> 4   ->  25 W
```

`<n>` is the magnitude of step n's answer, steps are separated by `;`, and the
limit is five. The answer pane prints the whole chain with each step's formula
and numbers, and TAB/VAR act on the **last** step, so a chain's diagram and
plot are about the quantity that answered the question.

Four rules, each there because the alternative is a half-run plan:

- a step may only reference an **earlier** step, so a plan cannot loop;
- a step that answers with more than one number (`x=0, x=3`) cannot feed the
  next one, and the plan is refused at that step;
- a plan over five steps is refused rather than truncated;
- units ride along for display but only magnitudes are passed. That is honest
  today and is exactly what slice 4's dimensional layer fixes: `5 N` flowing
  into a slot meaning `5 kg` is currently undetectable, which is why the refusal
  list is short rather than long.

What slice 1 does **not** yet include: the model emitting plans (it needs the
retrain this protocol was written for, and that is the next task), the 50-item
hand-written chain suite (ten chains are asserted today, in `test_plan`), and
plan verification before execution (S5) — steps are checked as they run, so a
bad plan fails at the step, not before it.

## Slice 2, as it stands

Two of the slice's items are in; the rest are untouched.

**W1, per-relation variance as a gate.** `stem-model/report_relations.py` scores
the corpus's `eval` split per relation, and `gate.sh` now fails when more
relations than a recorded budget fall below 50% on four or more rows. The
budget is 12 today against 10 actual, so it catches a wording change that robs
one relation to pay another without failing on the known state. The first table
it printed is why the rest of this slice is ordered the way it is: the deficit
is not spread evenly over 118 relations, it sits in a handful of
argument-selection failures (`arith_seq`, `geom_seq`, `moles`, `gas_p`).

**Digit repair, the smallest piece of W4's neighbourhood.** A wrong answer from
hand-written wording is usually the right relation with one number a keystroke
off — `0.1 mol in 0.25 L` comes back as `0.2`, `88 g at 44 g/mol` as `84` and
`48`. `runtime/repair.c` snaps such an argument back to the one number in the
sentence with the same sign, the same digit length and exactly one different
character, and refuses when two candidates are that close. Measured on the
1,942-question eval split, against a clean build of the commit before it:

| | Before | After |
| --- | --- | --- |
| Exact call | 65.3% | 65.9% |
| Questions refused as uncertain | 381 | 364 |
| Relations under 50% on 4+ rows | 11 | 10 |

Of the 23 calls it changed, 12 became correct, 0 broke a call that was already
correct, and 5 turned a refusal into a confidently wrong answer — every one of
them a question where the *other* argument was the wrong one, so the repair
fixed the number and removed the refusal that was doing useful work.
Harry's call on 2026-11-12 was to keep it: 12 right for 5 wrong in 1,942 is
worth the trade, and the guards stay because they are what makes the ratio that
good. The number to watch is the wrong-answer count, not the refusal count. The rules are deliberately timid because of
it: at most one argument per call, never a number the call already carries
(`pythagoras 2 7` is one character from `7 7`), never across a sign change, and
never two equally close candidates.

**W20, measured rather than assumed.** `report_relations.py --arg-audit` splits
every miss three ways and, for the middle one, records which argument position
was wrong and whether the wrong value appears verbatim in the question. On the
eval split: 1,280 correct, 251 the right relation with the wrong numbers, 411
the wrong relation. Of the 402 offending arguments in that middle class, 352
(88%) were copied straight out of the sentence and 50 were invented. The worst
offenders are argument-order problems, not arithmetic: `density` puts the volume
where the mass goes (14 from text, 1 invented), `arith_seq` (12/1),
`dilution_v1` (11/1), `calorimetry_c` (13/2), `work` (13/2).

That single number settles the rest of this slice. Grounding structurally cannot
catch a number that really is in the sentence, and no repair can either, because
nothing about the call looks wrong. The failure is slot assignment, which is a
model problem, not a runtime one.

**Two code-only items measured and rejected.** W4's number words and W6's
engineering prefixes are listed as cheap because they look like text munging. As
a pre-pass on the prompt they are not cheap, they are harmful:

| Change | Exact call |
| --- | --- |
| Nothing (shipped state) | 65.9% |
| Number words digitised, bare "one" left alone | 64.6% |
| Number words digitised, bare "one" included | 63.9% |

The model was trained on the words, so digitising them hands it an input
distribution it has never seen. W6 has no corpus presence at all — `4.7k` and
`2 MΩ` appear in none of the 1,942 eval rows — and a naive multiplier rule eats
molarity ("20 mL of 3 M acid" becomes "3000000 acid"), so any such code would be
unmeasured and dangerous at once.

What is genuinely left of slice 2 is the retrain and the lexicon: W2's slot
synonyms, W3's typo tolerance, W5's unit values, W7's polarity words. The
slice's own success criterion — hand-written accuracy up 8 points, per-relation
floor above 60% — is not met and cannot be met by runtime changes, for the
reason the argument audit gives: 88% of the wrong arguments are legitimately in
the sentence. Those four items need the model to know which slot a phrase fills.

## What this needs that the project does not have

- **A multi-step corpus.** Every training row today is one question and one
  call. Plans need questions, target plans, and a verifier — the parity harness
  is the verifier, which is why slice 1 is a protocol change rather than a data
  problem.
- **A limit engine**, for the convergence tests 4.2 skipped and for series work
  beyond pattern matching.
- **A dimension table** per relation, 136 lines of data that turn every plan
  into something checkable.
- **A real-exam regression suite** (M15) so "more capable" is a number and not a
  feeling.
- **A device run.** Every claim in this plan is host-verified until someone puts
  the `.tns` on a CX II and times a two-step answer.

## Deliberately not planned

- **A full CAS.** Symbolic simplification and arbitrary algebra is a different
  project, and the honest refusals are part of the product.
- **3D graphing, matrices beyond 3×3 with exact arithmetic, and CAS-level
  calculus identities.** Each is measurable against a CAS and loses.
- **A bigger vocabulary.** It was tried in 1.3 and cost 4.6 points of accuracy;
  nothing here depends on it.
