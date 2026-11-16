"""Tier 1 as data: which exam topics this program already serves, and how.

The point of this file is that "AP Precalculus is mostly covered" stops being
an opinion. Every topic an exec level names is either served by a registered
relation, served by the typed path with no model, or recorded as a gap, and
`course_audit.py` fails when a claim names a relation that does not exist.

Three kinds of claim, and the difference matters:

  relation   a registered relation answers this question shape. The claim is
             checkable against RELATIONS, which is what makes this file a
             contract rather than prose.
  typed      the expression path answers it with no model: `integrate(x^2,0,3)`,
             `d/dx`, `det`, and so on. The command is recorded so the claim can
             be read by a person, and its form is checked against what
             `pipeline_typed_answer` actually dispatches.
  gap        nothing answers it yet. Gaps are the work list, and they are worth
             writing down precisely because the alternative is remembering.

A relation may serve more than one course, so lookup is a set, not a label.

`exact_floor` is the recorded hand-written accuracy for a course, not a target:
it is the number the corpus measured when the row was written, and `gate.sh`
fails when a course falls more than `FLOOR_SLACK` below it. A budget that is
moved must move with the measurement in the same commit.
"""

import collections

from relations import RELATIONS

Course = collections.namedtuple("Course", "code name units exact_floor note")
Unit = collections.namedtuple("Unit", "number title topics")
Topic = collections.namedtuple("Topic", "topic serves")

# What pipeline_typed_answer dispatches before it falls back to evaluating an
# expression. A typed claim has to start with one of these, or be an expression
# the evaluator is expected to answer.
TYPED_FORMS = (
    "d/dx", "diff(", "derivative(",
    "integrate(", "int(",
    "series(", "taylor(",
    "ode(", "desolve(",
    "det(", "inv(", "rank(", "solve(",
)

# How far below the recorded number a course may drift before the gate fails.
# Small enough to catch a real regression, large enough that a different sample
# of the same corpus does not flip it: on ~2,000 rows one point is about 45
# questions, so two is well outside sampling noise.
FLOOR_SLACK = 2.0

# A course needs this many scored rows before its floor is allowed to judge it.
MIN_ROWS = 60


def rel(*names):
    """One claim per relation name, as a list so units can be concatenated."""

    return [Topic(n, ("relation", n)) for n in names]


def typed(topic, command):
    return Topic(topic, ("typed", command))


def gap(topic):
    return Topic(topic, ("gap", ""))


COURSES = collections.OrderedDict()


def course(code, name, units, exact_floor=None, note=""):
    COURSES[code] = Course(code, name, units, exact_floor, note)


course("ap-precalc", "AP Precalculus", [
    Unit("1", "Polynomial and rational functions",
         rel("quad_solve", "factor_quadratic", "complete_square", "cubic_solve")
         + [gap("rational function end behaviour and asymptotes"),
            gap("average rate of change over an interval")]),
    Unit("2", "Exponential and logarithmic functions",
         rel("exp_solve", "log_eval", "ln_eval", "log10_eval",
             "compound_interest", "continuous_growth", "half_life",
             "doubling_time")),
    Unit("3", "Trigonometric and polar functions",
         rel("sin_eval", "cos_eval", "tan_eval", "right_triangle",
             "law_sines", "law_cosines", "arc_length", "sinusoid")
         + [gap("polar coordinates and polar graphing")]),
    Unit("4", "Functions involving parameters, vectors, and matrices",
         rel("linear2", "arith_seq", "geom_seq")
         + [typed("2x2 determinant", "det(1,2;3,4)"),
            typed("2x2 inverse", "inv(1,2;3,4)"),
            gap("vector arithmetic and components")]),
], exact_floor=61.6,
   note="529 corpus eval rows, 2026-11-16")

course("ap-calc-ab", "AP Calculus AB", [
    Unit("1", "Limits and continuity",
         [gap("limits, one-sided limits, continuity")]),
    Unit("2", "Differentiation: definition and fundamentals",
         rel("deriv_poly3")
         + [typed("derivative of an expression", "d/dx(x^2-3x+1)"),
            gap("tangent line at a point")]),
    Unit("3", "Composite, implicit, and inverse functions",
         [gap("chain rule inside a word problem"),
          gap("implicit differentiation")]),
    Unit("4", "Contextual applications of differentiation",
         [gap("related rates"),
          gap("optimization"),
          gap("straight-line motion analysis")]),
    Unit("5", "Analytical applications of differentiation",
         [gap("critical points and monotonicity"),
          gap("mean value theorem")]),
    Unit("6", "Integration and accumulation of change",
         rel("integr_poly3")
         + [typed("definite integral", "integrate(x^2,0,3)"),
            typed("antiderivative", "integrate(x^2)"),
            gap("Riemann sums and trapezoid")]),
    Unit("7", "Differential equations",
         [typed("separable, closed form", "desolve(2y,0,1)"),
          typed("numeric solution", "ode(2y,0,1,1,64)")]),
    Unit("8", "Applications of integration",
         [gap("area between curves"),
          gap("volume of revolution"),
          typed("average value of a function", "integrate(x^2,0,3)/3")]),
], note="46 corpus eval rows, below the 60-row floor; P3 raises it")

course("ap-calc-bc", "AP Calculus BC", [
    Unit("1", "All of AP Calculus AB",
         [typed("derivative", "d/dx(sin(x))"),
          typed("definite integral", "integrate(x^2,0,3)"),
          gap("limits and continuity")]),
    Unit("9", "Parametric, polar, and vector-valued functions",
         [gap("polar area"),
          gap("parametric arc length"),
          gap("derivative of a parametrised curve")]),
    Unit("10", "Infinite sequences and series",
         [typed("Taylor and Maclaurin polynomials", "series(exp(x),0,5)"),
          gap("convergence tests"),
          gap("alternating series error bound")]),
], note="no corpus rows: BC is typed-engine work, judged by test_expr")

course("sat-act", "SAT / ACT maths", [
    Unit("1", "Algebra",
         rel("linear2")
         + [typed("evaluate an expression", "3(x+2)-4"),
            gap("linear equations with one unknown from text")]),
    Unit("2", "Advanced maths",
         rel("quad_solve", "factor_quadratic", "exp_solve",
             "average", "percent_error", "sig_figs", "scientific_notation")
         + [gap("percent change and percent of"),
            gap("ratios and proportions")]),
    Unit("3", "Problem solving and data analysis",
         rel("continuous_growth", "half_life")
         + [gap("mean, median and standard deviation from a list"),
            gap("probability and expected value"),
            gap("line of best fit and prediction")]),
    Unit("4", "Geometry and trigonometry",
         rel("pythagoras", "sin_eval", "cos_eval", "tan_eval")
         + [gap("area, perimeter and volume of common shapes"),
            gap("circles: arc, sector and radians")]),
    Unit("5", "Units and measurement",
         rel("km_to_m", "m_to_cm", "kg_to_lb", "liters_to_ml",
             "celsius_to_fahrenheit", "fahrenheit_to_celsius")),
], exact_floor=67.2,
   note="460 corpus eval rows, 2026-11-16")

course("intro-hs", "Intro high-school physics and chemistry", [
    Unit("1", "Kinematics",
         rel("kinematic_v", "free_fall", "proj_range")),
    Unit("2", "Forces and motion",
         rel("newton2", "friction_force", "circular_accel", "energy_friction")),
    Unit("3", "Energy, work and power",
         rel("kinetic_energy", "potential_energy", "work", "power",
             "spring_energy")),
    Unit("4", "Momentum and collisions",
         rel("momentum", "impulse")
         + [gap("elastic and inelastic collisions")]),
    Unit("5", "Circuits",
         rel("ohm_v", "ohm_i", "ohm_power", "parallel_resistance")
         + [gap("series resistance and network reduction")]),
    Unit("6", "Waves and sound",
         rel("wave_speed")),
    Unit("7", "Gases and thermochemistry",
         rel("boyles", "charles", "combined_gas", "gas_p",
             "specific_heat", "calorimetry_c", "calorimetry_mass")),
    Unit("8", "Solutions and stoichiometry",
         rel("molarity", "dilution", "dilution_v1", "molality", "moles",
             "stoichiometry", "stoich_mass", "percent_yield")
         + [gap("limiting reagent and empirical formula")]),
    Unit("9", "Acids and bases",
         rel("ph", "poh", "ph_from_poh", "h_from_ph")),
], exact_floor=64.2,
   note="752 corpus eval rows, 2026-11-16")


def iter_topics():
    """(course_code, unit_number, topic) for every claim, in file order."""

    for code, entry in COURSES.items():
        for unit in entry.units:
            for topic in unit.topics:
                yield code, unit.number, topic


def courses_for(relation):
    """Every Tier 1 course this relation serves, as a set of codes."""

    out = set()
    for code, _, topic in iter_topics():
        if topic.serves[0] == "relation" and topic.serves[1] == relation:
            out.add(code)
    return out


def counts(code):
    """(topics, relation, typed, gap) for one course."""

    entry = COURSES[code]
    total = served = typed_n = gap_n = 0
    for unit in entry.units:
        for topic in unit.topics:
            total += 1
            kind = topic.serves[0]
            if kind == "relation":
                served += 1
            elif kind == "typed":
                typed_n += 1
            else:
                gap_n += 1
    return total, served, typed_n, gap_n


def all_count():
    """The same four numbers across every course."""

    total = served = typed_n = gap_n = 0
    for code in COURSES:
        t, s, ty, g = counts(code)
        total += t
        served += s
        typed_n += ty
        gap_n += g
    return total, served, typed_n, gap_n


def unclaimed_relations():
    """Registered relations no Tier 1 course claims, for information only.

    This is not a failure: the catalogue deliberately serves courses outside
    Tier 1, and optics is not part of AP Precalculus.
    """

    claimed = {t.serves[1] for _, _, t in iter_topics()
               if t.serves[0] == "relation"}
    return sorted(set(RELATIONS) - claimed)
