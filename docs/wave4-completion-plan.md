# Finishing wave 4: 4.3's remainder and all of 4.4

`docs/wave4-plan.md` is the shape of the wave and where each piece stopped. This
page is the plan for the two pieces that are still open — the rest of the
differential-equation workstream and the last block of formula breadth — with
the costs measured rather than guessed, and the one part of it that a machine
without the Ndless toolchain cannot decide.

Two workstreams, two very different kinds of work:

| | 4.3 remainder | 4.4 breadth |
| --- | --- | --- |
| What it touches | typed-expression engine in `runtime/expr.c` | the relation catalogue, the fixed-point solvers, the training set |
| Does it need a retrain | no | **yes, mandatory** — see below |
| Can this machine verify it | yes: RK4 cross-check and round-trip gates | partly: parity, steps and rendering run here; accuracy needs a retrain |
| Blocked on | nothing | the capacity decision, but only for how good the result is, not for whether the code can be written |

## Part A — 4.3, the half that is left

`desolve` answers `y' = a*y + c` and the logistic `a*y + b*y^2`, which covers
growth, decay, Newton cooling and population growth. Everything else in the
first-order syllabus is refused. Three gaps and one display task:

### A1. Separable equations, `y' = g(x)*h(y)`

Decompose `f(x,y)` into a product of an x-only factor and a y-only factor, then
`∫dy/h(y) = ∫g(x)dx`. The x-side and the y-side are both antiderivatives, so
this is 4.1's integrator applied twice — the dependency the plan predicted.

Inverting the y-integral is the part that decides what can be answered:

| `h(y)` | `∫dy/h` | Explicit `y` | Status |
| --- | --- | --- | --- |
| constant | `y/h` | linear, already covered | not new |
| `y` | `ln|y|` | exponential, already covered | not new |
| `y^2` | `-1/y` | logistic, already covered | not new |
| `1/y` | `y^2/2` | `y = sqrt(2*G(x) + C)` | **new** — the case `y' = x/y` |
| `c*y + k` | `ln|cy+k|/c` | exponential with a shift | **new** — this is `desolve` with a non-zero constant, already reachable but by a different route |

So the shape of the work is: split into `g(x)h(y)`, integrate both sides,
recognise the y-antiderivative as one of a small set of invertible forms, and
build the explicit solution. Refuse everything else — `h(y) = sin(y)` has an
integral that inverts to nothing printable, and handing back an implicit
relation would be a coin flip dressed as an answer.

The inversion rule that carries `y' = x/y` is worth stating because it is the
one that needed a decision: `y = ±sqrt(...)` has two branches and the initial
condition picks one. Emit the branch the sign of `y0` selects, and refuse when
`y0 = 0`, where the branch is undefined and the equation is not separable in the
way the student wrote it.

### A2. Linear with an x-dependent coefficient, `y' = P(x)*y`

`y = y0*exp(∫P)`, and 4.1 can integrate `P` for any `P` it can integrate at all.
This is the general first-order homogeneous linear case, and it covers variable
decay rates (`y' = -x*y`), the classic `y' = y/x`, and anything else whose rate
integrates to something printable.

`y' = P(x)y + Q(x)` with **both** x-dependent is not in scope, and the reason is
numeric rather than conceptual: the solution needs `∫Q*exp(-∫P)dx`, which means
the integrator has to integrate a product of an arbitrary function with an
exponential of an antiderivative. That is by-parts territory the rule table does
not have. Refuse it, and say so in the answer rather than answering the
homogeneous part of a non-homogeneous equation, which is a wrong answer.

### A3. Slope-field data (4.3d)

`gexpr_slope_field(f, x0, x1, y0, y1, cols, rows, out)` fills a grid of
normalised segment angles from the same evaluator RK4 uses. Nothing draws it
yet — that is 4.6f and it needs device UI — but the host gate can check that
every segment's angle is `atan(f(x,y))`, which is the part that would otherwise
be silently wrong when the drawing code finally arrives.

### A4. The gate

The same trick that made the constant-coefficient work trustworthy: **compare
the closed form against RK4 at several points**, not against a formula written
by hand. Two independently derived methods agreeing is evidence; a formula
matching itself is not. Concretely, for each new shape:

- `desolve` the equation, evaluate the answer at 2–3 points, run RK4 with a
  small step, require agreement to four significant digits.
- check the refusals: `y' = x + y` (linear, non-homogeneous), `y' = sin(y)`,
  `y' = x*y/(1+y)` must all return the unsupported code rather than a guess.

### What 4.3 still will not do

No second-order equations, no systems, no boundary-value problems, no implicit
solutions, and no numeric solution printed as a table on the device yet. Those
are honest absences, not oversights: each needs either machinery this engine
does not have (a heap, a symbolic limit, an integration by parts) or a display
that does not exist (4.5).

## Part B — 4.4, the last 34 relations

The rest of AP Physics 2. The catalogue is 102 relations; this brings it to 136,
which the 512-token vocabulary holds with room to spare (its ceiling is about
250 names).

### The 34

Written in the units a student is given them, because that is also what keeps
every intermediate inside the engine's numeric range — see "the range problem"
below.

**Waves and sound (7)**

| Name | Formula | Arguments | Answer |
| --- | --- | --- | --- |
| `sound_level` | `L = 10*log10(I/1e-12)` | I in W/m², 0.0001 and up | dB |
| `sound_intensity` | `I = 1e-12*10^(L/10)` | L in dB, 80 and up | W/m² |
| `doppler_freq` | `f' = f*v/(v - vs)` | f, v, vs | Hz |
| `beat_freq` | `f_b = |f1 - f2|` | f1, f2 | Hz |
| `standing_wave_freq` | `f_n = n*v/(2*L)` | n, v, L | Hz |
| `string_freq` | `f = sqrt(T/mu)/(2*L)` | T, mu, L | Hz |
| `double_slit_fringe` | `y = m*lam*L/(1000*d)` | m, lam in nm, L in m, d in mm | mm |

**Optics (6)**

| Name | Formula | Arguments | Answer |
| --- | --- | --- | --- |
| `snell_angle` | `th2 = asin(n1*sin(th1)/n2)` | n1, th1, n2 | degrees |
| `critical_angle` | `thc = asin(n2/n1)` | n1, n2 | degrees |
| `lens_image_distance` | `di = f*do/(do - f)` | f, do | cm |
| `lens_magnification` | `m = -di/do` | di, do | dimensionless |
| `mirror_image_distance` | `di = f*do/(do - f)` | f, do | cm |
| `index_from_speed` | `n = 3/v` | v in units of 1e8 m/s | dimensionless |

**Fluids (7)**

| Name | Formula | Arguments | Answer |
| --- | --- | --- | --- |
| `gauge_pressure` | `P = rho*g*h` | rho, h, g | Pa |
| `buoyant_force` | `F = rho*V*g` | rho, V, g | N |
| `continuity_velocity` | `v2 = A1*v1/A2` | A1, v1, A2 | m/s |
| `bernoulli_pressure` | `P2 = P1 + rho*(v1^2 - v2^2)/2 + rho*g*(h1 - h2)` | P1, v1, v2, h1, h2, rho | Pa |
| `volume_flow_rate` | `Q = A*v` | A, v | m³/s |
| `pascal_press` | `F2 = F1*A2/A1` | F1, A1, A2 | N |
| `submerged_fraction` | `x = rho_obj/rho_fluid` | rho_obj, rho_fluid | dimensionless |

**Modern physics (9)** — in eV, nm and atomic mass units, which is how the exam
states them and the only way the constants fit

| Name | Formula | Arguments | Answer |
| --- | --- | --- | --- |
| `photon_energy_ev` | `E = 1240/lam` | lam in nm | eV |
| `photoelectric_kev` | `K = 1240/lam - phi` | lam in nm, phi in eV | eV |
| `threshold_wavelength` | `lam_max = 1240/phi` | phi in eV | nm |
| `de_broglie_electron` | `lam = 1.226/sqrt(V)` | V in volts | nm |
| `bohr_energy_ev` | `E_n = -13.6/n^2` | n | eV |
| `bohr_transition_nm` | `lam = 1240/(13.6*(1/n1^2 - 1/n2^2))` | n1, n2 | nm |
| `mass_energy_mev` | `E = 931.5*m` | m in u | MeV |
| `half_life_fraction` | `f = 0.5^(t/t_half)` | t, t_half | dimensionless |
| `activity_bq` | `A = 0.693*N/t_half` | N, t_half | Bq |

**Thermo and gas (5)**

| Name | Formula | Arguments | Answer |
| --- | --- | --- | --- |
| `rms_speed` | `v = sqrt(3*R*T/M)` | T, M in kg/mol | m/s |
| `gas_density` | `rho = P*M/(R*T)` | P, M, T | kg/m³ |
| `adiabatic_work` | `W = (P1*V1 - P2*V2)/(gamma - 1)` | P1, V1, P2, V2, gamma | J |
| `carnot_efficiency` | `eta = 1 - Tc/Th` | Tc, Th | dimensionless |
| `internal_energy_gas` | `U = 3*R*T/2` | T | J/mol |

### The range problem, and why the units are part of the design

`rat` is a pair of 64-bit integers normalised through a 9e15 limit, and
`parse_rat` accepts at most nine decimal places. There is no exponent field, so
an argument smaller than 1e-9 cannot be typed at all and a numerator or
denominator past 9e15 overflows.

That rules out the naive version of half of modern physics. Planck's constant is
6.626e-34 J·s: an argument of `6.626e-34` does not parse, and `h*c` in joules or
joule-metres overflows long before it renders. The same applies to Boltzmann's
constant in mean-free-path, which is why mean free path was **dropped** from the
plan rather than fudged — its formula squares a diameter of 3e-10 m, and 9e-20
does not fit in a denominator.

There is a second, tighter limit that only shows up in the generated data:
arguments travel as `str(value)` in the call text, so an argument written
`1e-05` reaches the device as those five characters and does not parse at all.
Every argument here is therefore an integer or a decimal of at least 0.0001,
which is why the sound relations take 80 dB and up and why `index_from_speed`
takes the speed in units of 1e8 m/s rather than metres per second.

The answer is not a wider integer width. It is that the exam already states these
quantities in units where the constants fold into a small number: photon energy
in eV and wavelength in nm gives `E = 1240/lam`, where 1240 is a constant and
nothing overflows. That is not a dodge around the engine's limits; it is the
form students are taught, and it makes the relation *easier* to trust because
the numbers on the screen are the numbers in the problem.

Every relation above is written so that the largest intermediate stays under
1e13 and the smallest above 1e-6. Where that was impossible the relation was
cut, and the cut is recorded rather than quietly shipped.

### The five places a relation has to exist

Measured on the existing catalogue, per relation:

| Place | File | Cost |
| --- | --- | --- |
| the answer, in Python | `relations/<subject>.py` | ~14 lines, plus the `step=` data |
| ten phrasings | `surface/<subject>.py` plus one `_ARG_NAMES` row | ~11 lines |
| the answer, on the device | `runtime/solvers_<subject>.c` plus one dispatch branch | ~27 lines |
| the masked token id | `runtime/reltable.h` | generated |
| the parity vector | `runtime/test_parity.c` | generated |

Two generated artifacts mean a relation cannot half-exist: the call-mask gate and
the parity gate both fail until the catalogue and the runtime agree.

### New numeric primitives

Three relations need an inverse sine, which the runtime does not have. It does
have `atan_deg9` in `solvers_trig.c`, so `asin` is `atan(x/sqrt(1-x^2))` and the
work is to move that helper to the shared boundary rather than write a second
inverse-trig routine that could disagree with the first. `log10_9`, `ln_9`,
`exp_9` and `sqrt12` already exist and cover the rest.

### The gate

- **Parity**: `gen_solver_parity.py` draws seeded instances and requires the C
  runtime to reproduce every byte of every Python answer. This catches the real
  failure mode of 34 relations written twice — the two copies drifting on one
  rounding case.
- **Steps**: `check_steps.py` evaluates each `step=expr` against the solver's own
  answer, so a wrong working line fails a gate instead of printing confidently.
- **Catalogue completeness**: the call-mask check compares `reltable.h` against
  the registry, and the parity script's gap lists catch a relation with no C
  solver and a C solver with no relation.
- **Accuracy**, which needs the retrain: see Part C.

Parity cannot catch a formula that is wrong in *both* languages. That gap is
closed by review against a formula table, and it is the one place in this plan
where the gate is a person reading code.

### Cut deliberately

- **Mean free path** — squaring 3e-10 m does not fit the numeric range.
- **Relativistic energy and momentum** — needs `sqrt(1 - v^2/c^2)` at v/c very
  close to 1, and the physics-BC depth is not there.
- **Nuclear binding energy** from per-nucleon tables — a data table, not a
  formula, and the model would have to memorise it.

## Part C — the retrain, and the decision it settles

### Why 4.4 cannot ship without one

`reltable.h` assigns each relation `REL_BASE + i` where `i` is its position in
`sorted(RELATIONS)`. Inserting `beat_freq` between `b` and `c` shifts every id
after it by one, so the call-token head the current weights learned points at
the wrong relation for every name past the first insertion. The mask would still
be internally consistent — it is generated from the same sorted list — so nothing
fails loudly. The model would simply start calling the wrong formulas.

Even if the ids were appended instead of sorted, a token the model never trained
on is a token it never emits. Either way: **new relations are unreachable until
the model is retrained**, and a catalogue that is larger but unreachable is a
worse product than the one on master.

So 4.4's definition of done includes a retrain, and no amount of writing 34
solvers substitutes for it.

### What the retrain costs, measured on this machine

| Step | Command | Cost |
| --- | --- | --- |
| dataset | `make data` | a few minutes, ~40 MB |
| train | `make train` | ~1 hour on CPU; the Mac mini has an MPS GPU, so appreciably less |
| quantise | `make export` | minutes, ~6 MB |
| score | `make eval`, `make eval-device` | minutes |

This machine has torch 2.15.0 with MPS available, verified by running it, so the
pipeline is runnable here rather than only in CI.

### The capacity decision

The evidence on master: accuracy on 1,840 hand-written rows is 72.6% at 84
relations, 73.8% at 90 and 72.3% at 102 — flat — while the generated gate fell
95.0%, 94.2%, 90.5%. More relations on the same 256-dimension model do not
degrade what a person types; they degrade how well the model generalises across
the phrasings it was trained on.

4.4 adds 34 relations, a third more catalogue. There are three honest options:

1. **Retrain at 256 first, measure, then decide.** One retrain, and it produces
   the number the decision needs: what a 136-relation catalogue does to both
   gates on this model. If the generated gate falls a few more points while the
   hand-written number holds, shipping is defensible and the cost is recorded.
2. **Widen to `--dim 352` and retrain.** Restores headroom at roughly twice the
   answer time on a 132 MHz ARM9 — a cost nobody has measured on hardware,
   because no machine here has a working device build.
3. **Write the relations, ship the catalogue, defer the retrain.** The solvers
   and gates all pass; the model cannot reach the new names. This is the option
   that looks like progress and is not, and it should only be chosen with eyes
   open.

The plan takes option 1: the retrain is part of the work, and it is run at the
current width so the measurement exists before anyone spends the answer time.
Option 2's cost is unmeasurable here, which is exactly why it should be decided
after a number exists rather than before.

### What the retrain cannot settle

Answer time on the device. Nothing in this project has ever been timed on real
hardware, so "twice the answer time" is an argument from the parameter count, not
a measurement. That stays unmeasured until someone runs the device build.

## Sequence

Ordered so that nothing waits on anything it does not need:

1. **A1–A3** — 4.3's remainder. No retrain, host-verifiable, and it closes the
   only workstream that a device could already use. **Done when** the RK4
   cross-check and the refusal cases pass.
2. **B1 relations and phrasings** — 34 relations with ten phrasings each. **Done
   when** the surface and registry consistency checks pass.
3. **B2 device solvers** — 34 C solvers against the fixed-point machinery, plus
   `atan_deg9` moved to the shared boundary. **Done when** parity reports zero
   mismatches and zero gaps.
4. **B3 steps** — the working line for each relation the gate can check. **Done
   when** `check_steps.py` passes.
5. **C retrain and measure** — dataset, train at the current width, quantise,
   score both gates, and record the numbers next to wave 3's.
6. **Documents** — the wave 4 status table and `docs/waves.md` updated with what
   actually shipped, including the accuracy cost if there is one.

Steps 1 and 2–4 touch disjoint files and can run in parallel. Step 5 is the wall
clock.

## Risks

**The numeric range**, which is the reason four relations were cut and the units
in Part B look the way they do. A relation whose intermediates overflow fails its
parity gate, so this shows up as a gate failure rather than a wrong answer — but
only if the drawn instances reach the extreme of each argument's range, which is
why the parity script draws 400 instances rather than 40 when a subject is new.

**Parity agreeing on a wrong formula.** The gate proves the two implementations
match, not that the physics is right. Mitigation is review, not tooling.

**The retrain regressing what works.** That is what the two accuracy gates
measure, and why the retrain happens before the documents claim anything.

**Device cost.** 34 more relations cost 34 more dispatch branches, and the ARM
build has to keep fitting. This is unverifiable here and stays on the risk list
until the cross-compile gate stops reporting SKIP.

## Definition of done for 4.4

- 136 relations registered, with no gap in either direction between the Python
  catalogue and the C runtime.
- `sh gate.sh quick` reports ALL GATES PASS, including parity, steps, call mask
  and the new differential-equation gates.
- A retrained model on 136 relations with both accuracy numbers recorded in
  `docs/waves.md`, next to wave 3's, whether they improved or not.
- The plan and status tables say what shipped and what the retrain cost, in the
  same place, so the next reader does not have to reconstruct it.


## What shipped, and what it cost

Everything above is on the branch, measured:

| | 102 relations (wave 3) | 136 relations (wave 4.4) |
| --- | --- | --- |
| generated held-out gate | 90.5% | **90.3%** |
| hand-written paraphrases | 72.3% | **71.8%** |
| wrong relation | — | 1.7% |
| fixed-point parity | 6/6 | **6/6** |
| full on-device path | — | 60/60 exact calls, 60/60 correct answers |
| step templates | 89 | **123** |
| solver parity cases | — | 54,400, no mismatch, no gap |

**The capacity prediction was wrong, and the reason is the interesting part.**
The plan said 34 more relations would cost accuracy because the generated gate
had fallen from 95.0% to 90.5% across 84 to 102 relations. At 136 it reads
90.3% — the fall stopped rather than continuing. The hand-written number is
within half a point too, measured on 1,942 rows that include hand-written
phrasings for all 34 new relations rather than only generated ones.

What *did* cost accuracy, by far more than the catalogue: the training recipe.
The same catalogue, trained with the Makefile's defaults as they were (6,000
steps, no paraphrase mix), scores 83.6% and 32.7%. So the two figures in this
project's history that looked like capacity limits are better explained as
protocol. `make data` now mixes the corpus's train split and `make train` runs
12,000 steps, which is the recipe the recorded numbers came from.

Three relations were cut rather than fudged, all for the numeric range recorded
above: mean free path, relativistic energy, and nuclear binding energy from
per-nucleon tables.

**Still unverified: the device.** Not one line of wave 4 has been compiled for
`arm926ej-s`, because no machine here has the Ndless toolchain. The ARM gate
reports SKIP. The wave's engine work is done and its catalogue is measured; its
product does not exist on a calculator yet.
