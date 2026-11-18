"""The catalogue's relations, with a formula where they demand a molar mass.

Each relation in `relations/chemistry.py` takes its arguments as numbers: a
molar mass, an ions-per-formula-unit count, a mole count worked out from a mass.
Those numbers are chemistry a student would have to look up, and they are the
reason five readable AP Chemistry bank rows stop at a missing argument.

These variants take the formula instead and compute the missing quantity from
the same table the rest of the app uses. They are reached by typing the call
(`moles_from_mass_formula 128 CH3OH`), so they need no model, no vocabulary and
no retrain. The numeric relation is untouched: a student who supplies the number
still gets the same answer.

Every answer is an exact rational of the decimal arguments and the integer
weights, rendered with `dec_exact`, which is the same renderer the C solver uses
for `dec4_render` - so the two print the same four significant digits.
"""

from fractions import Fraction

from relations.core import dec_exact

from .formula import FormulaError, ion_counts, molar_mass_mg

# The C renderer holds a numerator up to 9e15 and a denominator up to 9e17.
# A Ksp built from a small solubility can leave that range; the reference
# refuses it rather than printing a number the device cannot produce.
_MAX_NUM = 9 * 10 ** 15
_MAX_DEN = 10 ** 17

VARIANTS = (
    ("molar_mass_formula", ("formula",)),
    ("moles_from_mass_formula", ("number", "formula")),
    ("mass_from_moles_formula", ("number", "formula")),
    ("ions_from_moles_formula", ("number", "formula")),
    ("ksp_from_solubility_formula", ("number", "formula")),
    ("mole_fraction_formula", ("number", "formula", "number", "formula")),
    ("stoich_mass_formula", ("number", "formula", "number", "formula")),
)


def _number(text):
    try:
        value = Fraction(str(text))
    except (ValueError, ZeroDivisionError):
        raise FormulaError("not a number: " + str(text))
    return value


def _grams(mg, value):
    return value * Fraction(mg, 1000)


def _ions(text):
    cations, anions = ion_counts(text)
    return cations, anions


def molar_mass_formula(text):
    return dec_exact(Fraction(molar_mass_mg(text), 1000)) + " g/mol"


def moles_from_mass_formula(mass, text):
    mg = molar_mass_mg(text)
    return dec_exact(_number(mass) * 1000 / mg) + " mol"


def mass_from_moles_formula(moles, text):
    mg = molar_mass_mg(text)
    return dec_exact(_grams(mg, _number(moles))) + " g"


def ions_from_moles_formula(moles, text):
    cations, anions = _ions(text)
    return dec_exact(_number(moles) * (cations + anions)) + " mol ions"


def ksp_from_solubility_formula(solubility, text):
    cations, anions = _ions(text)
    s = _number(solubility)
    if s <= 0:
        raise FormulaError("a solubility must be positive")
    ksp = (s * cations) ** cations * (s * anions) ** anions
    if ksp.numerator > _MAX_NUM or ksp.denominator > _MAX_DEN:
        raise FormulaError("the solubility product is past what the device holds")
    return dec_exact(ksp)


def mole_fraction_formula(mass_a, formula_a, mass_b, formula_b):
    n_a = _number(mass_a) * 1000 / molar_mass_mg(formula_a)
    n_b = _number(mass_b) * 1000 / molar_mass_mg(formula_b)
    if n_a + n_b == 0:
        raise FormulaError("a mixture of nothing has no mole fraction")
    return dec_exact(n_a / (n_a + n_b))


def stoich_mass_formula(mass, formula_given, ratio, formula_product):
    given = molar_mass_mg(formula_given)
    product = molar_mass_mg(formula_product)
    moles = _number(mass) * 1000 / given
    return dec_exact(moles * _number(ratio) * product / 1000) + " g"


def solve(name, args):
    """One variant by its call name, for the parity fixture and the tests."""

    if name == "molar_mass_formula":
        return molar_mass_formula(*args)
    if name == "moles_from_mass_formula":
        return moles_from_mass_formula(*args)
    if name == "mass_from_moles_formula":
        return mass_from_moles_formula(*args)
    if name == "ions_from_moles_formula":
        return ions_from_moles_formula(*args)
    if name == "ksp_from_solubility_formula":
        return ksp_from_solubility_formula(*args)
    if name == "mole_fraction_formula":
        return mole_fraction_formula(*args)
    if name == "stoich_mass_formula":
        return stoich_mass_formula(*args)
    raise FormulaError("no such formula call: " + str(name))
