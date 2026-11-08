# Expansion plan, ranked by value

`capabilities.md` lists what is possible. `ROADMAP.md` lists tiers. This page
ranks the candidates by **value per unit of work**, so the next increment is a
decision rather than a menu.

Two different questions hide under "make it more powerful", and they rank
differently:

- **Capability** — how many questions can be answered at all.
- **Accuracy** — how often the answer that comes back is right.

Capability is bought in C, which costs microseconds on the device. Accuracy is
bought in data and retrains. Almost nothing here needs a bigger model.

## The four numbers behind the ranking

| Number | Value | Why it decides the order |
| --- | --- | --- |
| Prompt prefill | 43.9 tokens x 182 ms = **8.0 s** at 144 MHz, plus 1.4 s of decode | The documented "1.4 s per answer" counts generated tokens only. Prefill dominates, so prompt length and tokens/second are worth more than any solver feature |
| Vocabulary | 512 = 261 reserved + relations + merges; 66 relations leaves **185 merges**, **251 relations** is the hard ceiling | Breadth spends tokens. Every relation that can be removed by a general mechanism is a token refunded |
| Heap | 27.12 MiB total, 21.56 MiB largest allocation | Room for property tables in the hundreds of KB, never for a second model |
| Retrain cost | dataset, tokenizer, checkpoint — about 25 min plus an hour of wall time | C changes are free, vocabulary changes are not, so batches are spent deliberately |

The prefill figure is arithmetic on the recorded tok/s, not a device
measurement. Everything else in this page is downstream of it, so it is worth
settling with one hardware run before a latency plan is committed to.

## Ranked

Effort labels are estimates: **d** days, **w** weeks, **m** months.

### Tier S — days of work, no retrain, disproportionate value

| # | Change | Value | Effort |
| --- | --- | --- | --- |
| 1 | **Promote `graph.c`'s parser into a shared expression core**, with evaluation, Newton/bisection roots, and adaptive Simpson | Turns the device into a working calculus calculator for typed input at **zero inference cost**. Covers AP Calc AB outright and much of BC, and the 48-node Q16 parser with `+ - * / ^ sin cos tan` and implicit multiplication already exists | 1-2 w |
| 2 | **Fix the latency budget**: measure prefill on device, then widen the vocabulary to 1024 and thin the prompt | 8 s of a 9.4 s answer is prefill. A 1024-token vocabulary restores 462 merges at 300 relations, and more merges means a shorter prompt. This is the difference between "tolerable" and "unusable" and it is mostly a config change plus one retrain | d |
| 3 | **Constrained decoding**: a trie over relation name, arity, and numeric slots | An invalid call becomes structurally impossible. Removes a whole failure class with no retrain and no accuracy trade | d |
| 4 | **Edit-distance digit recovery** on the grounding guard | The dominant failure is mis-copying a digit (`4188` becomes `418`). Repairing a single unambiguous edit recovers answers the guard currently throws away — `sig_figs` sits at 80% for this reason alone | d |
| 5 | **Step display, keypad templates, error explanations** (D5, D8, D12) | The answer is usually already right; the interface is what makes it believed. Keypad templates also delete the worst part of the current UI, freehand typing on a calculator | d each |

Items 3 and 4 are the cheapest accuracy improvements available anywhere in the
plan. Both are pure C, both fit inside a week, neither needs a retrain.

### Tier A — one retrain each, multiplicative coverage

| # | Change | Value | Effort |
| --- | --- | --- | --- |
| 6 | **`solve_for(unknown)` over direction-agnostic equations** (B2) | The single biggest structural win. `ohms_law solve_for I` replaces `ohm_i`; one equation replaces every hand-written inverse. Roughly 2.5-4x the questions per relation slot, which is how AP coverage fits in a 1024-token vocabulary | 2-3 w |
| 7 | **Adversarial distractors and real wording** (A10, A9) | The dominant error is copying a plausible number, and the phrasing banks are synthetic. This is the lever that moves measured accuracy, and it is data plus a retrain, not code | d-w |
| 8 | **Force them into the gate** (F1, F4) | `stress_cases.py` and `evaluate_paraphrase.py` already exist as scripts. As gates with a per-relation dashboard they stop accuracy from silently regressing between relation batches | d |
| 9 | **Exact rational linear algebra**: n x n Gauss-Jordan, determinant, inverse, rank | Cheap on the existing rational helpers, and it unlocks equation systems, chemistry ICE tables, redox balancing, and Kirchhoff in one move | d |
| 10 | **Abstention from the call-token margin** (A1) | The product's stated rule is that refusals beat wrong answers. Today the guard enforces that by string matching; a margin turns silent wrong relations into explicit refusals | d |

### Tier B — subject breadth, predictable value per relation

Each relation is about 15 lines of Python and 86 of C plus a retrain, so these
are batches, ordered by exam weight rather than by elegance.

| # | Batch | Value | Effort |
| --- | --- | --- | --- |
| 11 | **AP Physics 1 mechanics** — friction, inclines, Atwood, circular force, energy conservation, collisions, torque, rotational inertia, SHM, standing waves, Doppler, lenses | The largest unserved subject. Twenty-eight relations is roughly a full AP 1 mechanics course | 1-2 w |
| 12 | **AP Chemistry core** — limiting reagent, titration, Ka/Kb/buffers, Ksp, ICE equilibrium, rate laws, Arrhenius, Hess's law, Nernst, electrochemistry | Second largest unserved subject, and `solve_for` makes most of it a formula and a dimension signature | 1-2 w |
| 13 | **Calculus AB extensions** — product/quotient/chain at a point, tangent and normal lines, critical points, concavity, area, volumes of revolution, average value, related rates | Depth where the tool is already strongest, and most of it is the expression core from #1 with a formatting layer | 1 w |
| 14 | **Thermodynamics** — calorimetry, latent heat, first law, p dV work per process, Carnot, COP, entropy, Fourier conduction, Stefan-Boltzmann, Newton cooling, v_rms | The thermodynamics catalogue is mostly formulas, so this is the highest yield per line of the physics work | 1-2 w |
| 15 | **AP Physics C E&M** — Coulomb, Gauss surfaces, potential, capacitors, RC, Kirchhoff, Biot-Savart, Ampere, Faraday, inductance, LC | Largest single batch at about thirty relations, and much of it reuses `solve_for` families | 2 w |
| 16 | **Units as a type** (B1) | Dimension exponents on every quantity. Without it, reusing one formula across unit systems is hand-written conversion relations; with it, the conversion graph is one table | 2-3 w |

### Tier C — real projects, start only when the tiers above are in use

| # | Change | Value | Effort |
| --- | --- | --- | --- |
| 17 | Symbolic integration — table, linearity, substitution, by parts, partial fractions | Completes BC, and it is the honest boundary between "calculator" and "CAS" | 3-6 w |
| 18 | Series: Taylor coefficients, convergence and error tests | The remaining BC unit, and it needs the expression core first | 2-3 w |
| 19 | ODE integration and separable solving | Cooling, kinetics, RC, logistic, population models | 1-2 w |
| 20 | Property tables: saturated water, cp, standard enthalpies, reduction potentials | Turns thermo and electrochemistry from formulas into answers, at the cost of curating licensed data | 1-2 w |
| 21 | Statistics and distributions: sd, regression, normal/t/chi-square quantiles, binomial, Poisson | Cheap on the existing arithmetic and outside the stated focus, but a large question class on its own | 1 w |

### Deferred, with the reason recorded

| # | Not doing | Why |
| --- | --- | --- |
| 22 | Multi-step call tape (A3) | Prefill is already 8 s; a chain pays it once and then pays decode per step. Encode multi-step physics as one arity-rich relation instead — the chain lives in C, where it is free, and the call grows by two or three tokens |
| 23 | Multi-function plotting, table view, inequality shading (D1, D2, D4) | Graph mode works. These are polish on a mode nobody has used on hardware yet |
| 24 | A wider model (A5) | 4x the latency for a routing job that the width ladder shows does not need it |
| 25 | Full CAS (B18) | Tens of thousands of lines, and it is a different product sharing a screen |
| 26 | On-device training, TI's CAS, free-response answering | Out of budget, out of reach, or a different product |

## What to do first

If the goal is the largest capability jump per week of work, the order is
**1, then 3 and 4, then 9**. Item 1 needs no model at all and converts the
device into a real calculus calculator; items 3 and 4 are two days that raise
the measured answer rate; item 9 is one day that unlocks systems, equilibrium,
and circuits.

If the goal is the largest *measured accuracy* jump, the order is **3, 4, 7, 8**.
Every one of them is either pure C or a data batch, and all four fit in a
fortnight.

If the goal is AP exam coverage as a catalogue, the order is **6, then 11-15**.
Nothing there is research; it is formula writing against a solved architecture.
