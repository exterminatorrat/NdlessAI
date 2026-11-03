"""Many phrasings per relation, so the model learns the wording is noise.

relations.py emits exactly one sentence per relation, which a model can memorise
word for word. Training on a single phrasing produces a model that scores 100%
on new numbers with old words and collapses on anything a person would actually
type. This module supplies the surface variety.

The templates intentionally differ in word order, verb, and notation, while
every template for a relation keeps the same numbers in the same roles, so the
correct call is unchanged.
"""

from relations import poly

TEMPLATES = {
    "quad_solve": [
        "Solve {p} = 0 for x.",
        "Find the roots of {p}.",
        "For what values of x does {p} equal zero?",
        "Solve the quadratic {p} = 0.",
        "What are the solutions to {p} = 0?",
        "Find x if {p} = 0.",
        "Determine the roots of {p}.",
    ],
    "deriv_poly3": [
        "Evaluate the derivative of f(x) = {p} at x = {x}.",
        "Find f'({x}) for f(x) = {p}.",
        "What is the slope of f(x) = {p} when x = {x}?",
        "Differentiate {p} and evaluate at x = {x}.",
        "Compute the derivative of {p} at x = {x}.",
        "At x = {x}, what is the rate of change of {p}?",
        "Evaluate d/dx of {p} at x = {x}.",
    ],
    "integr_poly3": [
        "Compute the definite integral of {p} from x = {lo} to x = {hi}.",
        "Integrate {p} from {lo} to {hi}.",
        "What is the area under {p} between x = {lo} and x = {hi}?",
        "Evaluate the definite integral of {p} with lower limit {lo} and upper limit {hi}.",
        "Find the integral of {p} over [{lo}, {hi}].",
        "Calculate the area under the curve {p} from {lo} to {hi}.",
    ],
    "linear2": [
        "Solve the system {a1}x + {b1}y = {c1} and {a2}x + {b2}y = {c2}.",
        "Find x and y given {a1}x + {b1}y = {c1} and {a2}x + {b2}y = {c2}.",
        "Two equations: {a1}x + {b1}y = {c1}, {a2}x + {b2}y = {c2}. Solve them.",
        "Solve simultaneously for x and y: {a1}x + {b1}y = {c1}; {a2}x + {b2}y = {c2}.",
        "Determine x and y that satisfy {a1}x + {b1}y = {c1} and {a2}x + {b2}y = {c2}.",
    ],
    "exp_solve": [
        "Solve {base}^x = {k} for x.",
        "If {base}^x = {k}, what is x?",
        "Find the exponent x such that {base}^x = {k}.",
        "What power of {base} gives {k}?",
        "Solve for x in the equation {base}^x = {k}.",
    ],
    "kinematic_v": [
        "A body starts at {v0} m/s with constant acceleration {a} m/s^2. What is its velocity after {t} s?",
        "A car speeds up from {v0} m/s at {a} m/s^2 for {t} s. How fast is it going now?",
        "An object moving at {v0} m/s accelerates at {a} m/s^2 for {t} s. Find its final velocity.",
        "Starting at {v0} m/s, a particle accelerates at {a} m/s^2 for {t} s. What is its speed?",
        "What is the final velocity of something launched at {v0} m/s that then accelerates at {a} m/s^2 for {t} s?",
    ],
    "proj_range": [
        "A projectile is launched at {v0} m/s at {theta} degrees above the horizontal. What horizontal range does it cover with g = {g} m/s^2?",
        "How far does a ball fly if kicked at {v0} m/s at {theta} degrees, with g = {g}?",
        "Find the horizontal distance travelled by a projectile launched at {v0} m/s, {theta} degrees, g = {g}.",
        "A stone is thrown at {v0} m/s, {theta} degrees above level ground, with g = {g}. How far does it land?",
        "What is the range of a launch at {v0} m/s and {theta} degrees if gravity is {g} m/s^2?",
    ],
    "ohm_i": [
        "A {R} ohm resistor has {V} V across it. What current flows?",
        "A {V} V battery is across a {R} ohm resistor. What current flows?",
        "How many amps flow through {R} ohms with {V} volts applied?",
        "Find the current when {V} V is applied to a {R} ohm resistor.",
        "With {V} volts across {R} ohms, what is the current?",
    ],
    "moles": [
        "How many moles are in {mass} g of a substance with molar mass {mm} g/mol?",
        "Convert {mass} g of a substance with molar mass {mm} g/mol into moles.",
        "How many mol are in {mass} grams when one mole weighs {mm} g?",
        "A sample of {mass} g has molar mass {mm} g/mol. How many moles is that?",
        "Find the number of moles in {mass} g given a molar mass of {mm} g/mol.",
    ],
    "dilution": [
        "What is the molarity after diluting {v1} mL of {c1} M solution to a final volume of {v2} mL?",
        "Dilute {v1} mL of {c1} M solution up to {v2} mL. What is the new concentration?",
        "You take {v1} mL of a {c1} M solution and add water to {v2} mL. Find the molarity.",
        "What concentration results from bringing {v1} mL of {c1} M up to {v2} mL?",
        "{v1} mL of {c1} M is diluted to {v2} mL. What molarity do you get?",
    ],
    "gas_p": [
        "Find the pressure of {n} mol of ideal gas at {T} K in {V} L. Use R = 0.08206 L atm / (mol K).",
        "What pressure do {n} mol of gas exert at {T} K in {V} L? Use R = 0.08206.",
        "Calculate the pressure of {n} mol of ideal gas occupying {V} L at {T} K, R = 0.08206.",
        "With {n} mol at {T} K in {V} L, what is the pressure? Take R = 0.08206.",
        "Use the ideal gas law to get the pressure for {n} mol, {T} K, {V} L, R = 0.08206.",
    ],
}


_ARG_NAMES = {
    "quad_solve": ("a", "b", "c"),
    "deriv_poly3": ("a", "b", "c", "d", "x"),
    "integr_poly3": ("a", "b", "c", "d", "lo", "hi"),
    "linear2": ("a1", "b1", "c1", "a2", "b2", "c2"),
    "exp_solve": ("base", "k"),
    "kinematic_v": ("v0", "a", "t"),
    "proj_range": ("v0", "theta", "g"),
    "ohm_i": ("V", "R"),
    "moles": ("mass", "mm"),
    "dilution": ("c1", "v1", "v2"),
    "gas_p": ("n", "T", "V"),
}

_POLY_COEFFS = {"quad_solve": 3, "deriv_poly3": 4, "integr_poly3": 4}


def render(name, params, index):
    """Render `params` (in relation argument order) with phrasing `index`."""

    fields = dict(zip(_ARG_NAMES[name], params))
    if name in _POLY_COEFFS:
        fields["p"] = poly(list(params[:_POLY_COEFFS[name]]))
    bank = TEMPLATES[name]
    return bank[index % len(bank)].format(**fields)


def count(name):
    return len(TEMPLATES[name])
