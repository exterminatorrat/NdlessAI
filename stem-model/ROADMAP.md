# Roadmap

What the system can do, what it could do, and what each addition costs. Ordered
by value per unit of work, not by ambition.

## The shape of the thing

The model reads English and picks a tool. It never does mathematics. Every
answer comes from C code that computes exactly.

    question -> model picks a call -> C solver computes -> answer

So "make it more powerful" means "add tools", not "make the model bigger". A
5.2M-parameter model choosing among 100 tools is barely harder to train than
one choosing among 11. The vocabulary has room: 100 relations still leaves 151
BPE merges for text.

## Where it stands

102 relations across ten subject modules, all verified byte-identical between
the Python reference and the C solvers, with every C test and parity gate
passing (`sh gate.sh`). On the 1,840 hand-written rows the model reads 72.3%
exact and 9.8% wrong relation; the generated-phrasing gate is 90.5%, down from
95.0% at 84 relations, because the catalogue outgrew the model rather than the
training budget — `../docs/waves.md` records that measurement and what it
implies. See `PROGRESS.md` for the per-relation state and `TIER1_PLAN.md` for
the design.

The subject list below is the original 66-relation Tier 1 baseline, kept because
the tier descriptions refer to it; `../docs/status.md` lists the current
catalogue by module.

| Subject | Relations |
|---|---|
| Calculus | derivative of a cubic at a point; definite integral of a cubic |
| Precalc | quadratic roots; 2x2 linear system; `b^x = k` |
| Trig | sin, cos, tan; Pythagoras; law of sines; law of cosines; arc length |
| Logs | log base b; ln; log10; compound interest; continuous growth; half-life; doubling time |
| Algebra | arithmetic and geometric sequences; absolute value; linear inequality; 2x2 system; completing the square; factoring |
| Physics | kinetic and potential energy; work; power; momentum; Newton's second law; centripetal acceleration; wave speed; specific heat; Ohm's law (V and P); free fall; kinematics; projectile range |
| Chemistry | molarity; pH; pOH; pH from pOH; percent yield; Boyle; Charles; combined gas; density; stoichiometry; moles; dilution; ideal gas |
| Units | km/m; m/cm; Celsius/Fahrenheit both ways; kg/lb; L/mL; percent error; scientific notation; mean; significant figures |

Nothing from the Tier 1 catalogue is deferred. `right_triangle`, `sinusoid`,
`cubic_solve` and `sig_figs` were each blocked on a missing helper (`atan`, a
multi-value output, rational-root deflation, an n-significant-figure
formatter); all four helpers now exist.

## Tier 1: breadth (weeks, model unchanged)

Each relation is roughly 15 lines of Python plus about 86 lines of C. The
model retrains in under an hour on a laptop GPU.

**Trigonometry** — sin/cos/tan of an angle, inverse trig, law of sines, law of
cosines, amplitude/period of a sinusoid. Unlocks the most problems per
relation added.

**Logs and exponentials** — log rules, change of base, compound interest,
exponential growth and decay, half-life.

**More algebra** — cubic and quartic roots, factoring, completing the square,
arithmetic and geometric sequences, systems of three equations.

**More physics** — kinetic and potential energy, momentum and collisions,
work and power, Newton's second law, circular motion, waves, specific heat.

**More chemistry** — balancing equations, stoichiometry, limiting reagent,
pH and pOH, molarity, percent yield, gas law variants (Boyle, Charles,
combined).

**Units and constants** — dimensional conversion, significant figures,
physical constants table.

Target: 100 relations covering a high-school curriculum. This is the tier that
makes the thing genuinely useful, and it is data entry plus formula writing.

## Tier 2: graphs (days, no model at all)

The calculator has `lcd_blit` and a 320x240 screen. Plotting is pure C and
needs no inference.

- Plot `y = f(x)` for a polynomial or trig expression
- Zoom and pan with the arrow keys
- Trace a point, show coordinates
- Plot a solved function alongside its roots

This is the highest value per day of work in the whole roadmap, because it
needs no training and no model changes.

## Tier 3: an expression engine (months)

The jump from "pick a formula" to "understand an expression". Parse
`3x^2*sin(x)`, build a tree, differentiate symbolically, evaluate numerically.

- Tokeniser and parser for infix expressions with precedence
- Symbolic differentiation by the standard rules
- Numeric evaluation, including a root finder
- Numeric integration (Simpson, adaptive)

This is real engineering rather than data entry. It is also where "advanced
calculus" starts to become honest, because it handles expressions nobody
anticipated rather than a fixed list.

## Tier 4: a computer algebra system (a project in itself)

Symbolic integration, arbitrary algebra, simplification. SymPy is roughly
500,000 lines of Python; a small C CAS is still tens of thousands of lines.

Possible, but it is a different project that happens to share a screen. Do not
start it before tiers 1 and 2 are done and in use.

## What will not happen

**General reasoning.** 20M parameters at 198 MHz is about 4 seconds per token
and no capacity for open-ended problem solving. The hardware sets this, not
the design.

**Reaching the OS computer algebra system.** Ndless apps cannot call TI's CAS.
Everything is ours or nothing.

**A bigger model as the answer.** Going from 5.2M to 20M parameters costs 4x
the speed and buys very little, because the model's job is classification, not
knowledge.

## Recommended order

1. **Graphs** — days, no model, immediately visible
2. **Trig and logs** — the two highest-value relation batches
3. **Physics and chemistry breadth** — fills out the curriculum
4. **Expression engine** — only once the above is in use and the gaps are known

## Cost reference

| Item | Work | Model change |
|---|---|---|
| One relation | ~15 lines Python, ~86 lines C | retrain, under an hour |
| Graph plotting | days | none |
| Expression parser | weeks | none |
| Symbolic differentiation | weeks | none |
| Full CAS | months to years | none |

The model is not the bottleneck at any tier. The bottleneck is how much exact
mathematics we write.
