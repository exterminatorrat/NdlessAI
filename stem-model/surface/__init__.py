"""Many phrasings per relation, so the model learns the wording is noise.

relations.py emits exactly one sentence per relation, which a model can memorise
word for word. Training on a single phrasing produces a model that scores 100%
on new numbers with old words and collapses on anything a person would actually
type. This module supplies the surface variety.

The templates intentionally differ in word order, verb, and notation, while
every template for a relation keeps the same numbers in the same roles, so the
correct call is unchanged.
"""

from relations import amp, lin2, poly

from . import (  # noqa: F401
    precalc,
    calculus,
    algebra,
    logs,
    trig,
    physics,
    chemistry,
    units,
)

TEMPLATES = {
    **precalc.TEMPLATES,
    **calculus.TEMPLATES,
    **algebra.TEMPLATES,
    **logs.TEMPLATES,
    **trig.TEMPLATES,
    **physics.TEMPLATES,
    **chemistry.TEMPLATES,
    **units.TEMPLATES,
}

_ARG_NAMES = {
    "sin_eval": ("a",),
    "cos_eval": ("a",),
    "tan_eval": ("a",),
    "pythagoras": ("a", "b"),
    "law_sines": ("a", "A", "B"),
    "law_cosines": ("a", "b", "C"),
    "arc_length": ("r", "theta"),
    "log_eval": ("base", "x"),
    "ln_eval": ("x",),
    "log10_eval": ("x",),
    "compound_interest": ("P", "r", "n", "t"),
    "continuous_growth": ("P", "r", "t"),
    "half_life": ("initial", "hl", "t"),
    "doubling_time": ("r",),
    "arith_seq": ("a1", "d", "n"),
    "geom_seq": ("a1", "r", "n"),
    "abs_value_eq": ("a", "b", "c"),
    "linear_ineq": ("a", "b", "c"),
    "complete_square": ("a", "b", "c"),
    "factor_quadratic": ("a", "b", "c"),
    "kinetic_energy": ("m", "v"),
    "potential_energy": ("m", "h", "g"),
    "work": ("F", "d", "theta"),
    "power": ("W", "t"),
    "momentum": ("m", "v"),
    "newton2": ("m", "a"),
    "circular_accel": ("v", "r"),
    "wave_speed": ("f", "lam"),
    "specific_heat": ("m", "c", "dT"),
    "ohm_v": ("I", "R"),
    "ohm_power": ("I", "V"),
    "free_fall": ("h", "g"),
    "molarity": ("n", "V"),
    "ph": ("H",),
    "poh": ("OH",),
    "ph_from_poh": ("poh",),
    "percent_yield": ("actual", "theoretical"),
    "boyles": ("P1", "V1", "V2"),
    "charles": ("V1", "T1", "T2"),
    "combined_gas": ("P1", "V1", "T1", "P2", "T2"),
    "density": ("m", "V"),
    "stoichiometry": ("moles_given", "ratio"),
    "km_to_m": ("km",),
    "m_to_cm": ("m",),
    "celsius_to_fahrenheit": ("c",),
    "fahrenheit_to_celsius": ("f",),
    "kg_to_lb": ("kg",),
    "liters_to_ml": ("L",),
    "percent_error": ("measured", "accepted"),
    "scientific_notation": ("mantissa", "exponent"),
    "average": ("a", "b", "c", "d"),
    "sinusoid": ("A", "B", "C"),
    "cubic_solve": ("a", "b", "c", "d"),
    "right_triangle": ("opp", "adj"),
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
    "dilution_v1": ("m1", "m2", "v2"),
    "molality": ("n", "kg"),
    "h_from_ph": ("ph",),
    "buffer_ph": ("pka", "base", "acid"),
    "calorimetry_mass": ("q", "c", "dT"),
    "calorimetry_c": ("q", "m", "dT"),
    "latent_heat": ("m", "L"),
    "first_law": ("q", "w"),
    "gibbs": ("dH", "T", "dS"),
    "carnot": ("t_hot", "t_cold"),
    "gas_p": ("n", "T", "V"),
    "sig_figs": ("value", "n"),
    "friction_force": ("mu", "N"),
    "work_energy": ("m", "v0", "v"),
    "power_force": ("F", "v"),
    "energy_friction": ("m", "v0", "f", "d"),
    "impulse": ("F", "t"),
    "spring_energy": ("k", "x"),
    "pendulum_period": ("L", "g"),
    "torque": ("F", "r", "theta"),
}

_POLY_COEFFS = {
    "quad_solve": 3,
    "deriv_poly3": 4,
    "integr_poly3": 4,
    "abs_value_eq": 2,
    "linear_ineq": 2,
    "complete_square": 3,
    "factor_quadratic": 3,
    "cubic_solve": 4,
}

_POLY_SLICE = {"sinusoid": (1, 3)}

_SYSTEMS = {"linear2"}

_COEF = {"sinusoid"}


def is_held_out(name, index):
    """Every fifth phrasing is reserved, spread across the list.

    Taking the last phrasing instead would make the held-out set
    systematically the oddest one and understate what the model learned.
    """

    return index % 5 == 4


def train_indices(name):
    return [i for i in range(len(TEMPLATES[name])) if not is_held_out(name, i)]


def held_out_indices(name):
    return [i for i in range(len(TEMPLATES[name])) if is_held_out(name, i)]


def render(name, params, index):
    """Render `params` (in relation argument order) with phrasing `index`."""

    fields = dict(zip(_ARG_NAMES[name], params))
    if name in _POLY_COEFFS:
        fields["p"] = poly(list(params[:_POLY_COEFFS[name]]))
    if name in _POLY_SLICE:
        lo, hi = _POLY_SLICE[name]
        fields["p"] = poly(list(params[lo:hi]))
    if name in _SYSTEMS:
        fields["e1"] = lin2(*params[:3])
        fields["e2"] = lin2(*params[3:6])
    if name in _COEF:
        fields["a"] = amp(params[0])
    if name == "sig_figs":
        fields["figs"] = "figure" if params[1] == 1 else "figures"
    bank = TEMPLATES[name]
    return bank[index % len(bank)].format(**fields)


def count(name):
    return len(TEMPLATES[name])
