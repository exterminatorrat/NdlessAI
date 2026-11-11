# Capabilities inventory

Every capability the project could add, grouped by the surface it touches, with
the work it costs and whether it needs a retrain. This is a catalogue, not a
plan: `ROADMAP.md` orders work by value, and this page makes sure nothing is
forgotten.

Nothing here is scheduled. An entry is a candidate, and several entries are
recorded deliberately as "not worth doing" so the reason survives.

## What already exists

Additions only make sense against a true baseline. Today the project has:

| Surface | Already built |
| --- | --- |
| Model | 5.25M-parameter int8 decoder, 6 layers, d=256, 512-token BPE, greedy decode, 20-token call budget |
| Contract | 102 fixed-arity relations, `CALL <name> <args>`, every argument must appear in the question |
| Solvers | Integer and exact-rational C for all 136 relations across eleven per-subject units, four-significant-digit rendering |
| Parser | Q16 expression parser for graphs: `+ - * / ^`, `sin cos tan sqrt abs ln exp`, `pi` and `e`, unary minus, decimals, `x`, 48 nodes; symbolic differentiation and numeric integration on the same tree, and exact rational `det`/`inv`/`rank`/`solve` |
| Graphing | Autoscale, pan, zoom, trace, root and extremum detection on a 320x240 framebuffer |
| Evaluation | Hand-written wording 72.3% exact and 9.8% wrong relation on 1,840 rows; generated gate 90.5% exact; C/PyTorch logit parity, ARM cross-compile check |
| Platform | `.tns` build, weight file loading, msgbox UI, BPE and pipeline shared with host tests |

The catalogue below assumes all of that stays.

## A. Model and inference

| # | Capability | What changes | Model | Work |
| --- | --- | --- | --- | --- |
| A1 | Abstention | An `unsupported` outcome from the call-token margin, plus out-of-scope training rows | retrain | days |
| A2 | Constrained decoding | Trie over relation, arity and numeric grammar so an invalid call cannot be emitted | retrain | days |
| A3 | Multi-step tape | `@r1` argument references, a call tape, and composition training rows | retrain | weeks |
| A4 | Longer calls | Raise the 20-token cap and the 128-token training window | retrain | days |
| A5 | Wider model | d=352 (9.6M) or d=512; measured at 3.0 tok/s vs 5.5 on the same hardware | retrain | weeks |
| A6 | Domain tokenizer | More merges, subject tokens, reserved tokens for units and `@` | retrain | days |
| A7 | Ranked alternatives | Top-3 candidate calls with margins, so a near miss is visible to the user | retrain | days |
| A8 | Better quantization | Group 16 or mixed precision on attention only; measure logit drift | retrain | days |
| A9 | Real typed wording | Hand-written and paraphrased questions in training, not just the phrasing bank | retrain | weeks |
| A10 | Adversarial distractors | Two or more same-unit quantities, values that belong to a different relation | retrain | days |
| A11 | Curriculum | Order training by relation difficulty; trim the 200k set to the rows that teach something | retrain | days |
| A12 | Edit-distance recovery | A character-level repair pass when grounding fails, for a mistyped digit | none | days |

A5 is listed for completeness and is the one entry the evidence argues
against: the repository's own width ladder shows shrinking costs 2.5 points on
grounded computation and 69 points on explanation, which means routing survives
small models.

## B. Mathematics core

| # | Capability | What changes | Model | Work |
| --- | --- | --- | --- | --- |
| B1 | Units as a type | Dimension exponents and an affine-offset flag on every quantity; refuse on mismatch | none | weeks |
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
linear algebra over integer stoichiometry, not a numeric call.

## D. Interaction and interface

| # | Capability | Notes |
| --- | --- | --- |
| D1 | Multi-function plotting | Several curves with distinct colours and a legend |
| D2 | Graph overlays | Shade the definite integral, draw the tangent, mark roots and extrema |
| D3 | Table view | `x` and `f(x)` columns, scrollable |
| D4 | Inequality shading | Parse `< <= > >=` and fill the region |
| D5 | Step-by-step display | Show the call, the formula, the substitution, the result — no model involvement |
| D6 | History | Scrollable log of questions and answers, persisted to a file |
| D7 | Answer reuse | Send the last answer into the next question as a slot value |
| D8 | Template keypad | Freeform text input is the worst part of the current UI; a keypad grid of relation templates removes it |
| D9 | Practice mode | Generated problems by subject and difficulty, with marking |
| D10 | Spaced repetition | Track per-relation accuracy per user and re-ask the weak ones |
| D11 | Unit prompt | When units are ambiguous, ask which unit the answer should use |
| D12 | Error explanations | "I matched this to Ohm's law, but 12 does not appear in the question" |
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
project has not yet touched — D8 as three scrollable pages of typed syntax
rather than a keypad grid, and D12 for the three pipeline failures. D2 and D3
are the graph overlays and the table view: the shaded integral and the marked
answer exist in 4.6, the scrollable table does not.

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
