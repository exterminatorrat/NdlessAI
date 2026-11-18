"""The variants that take a formula where the catalogue takes a number.

Two properties are tested together: the derived quantity is the chemistry the
student would have looked up, and a formula the app cannot read refuses instead
of answering. The last test pins the numeric relations, because a variant that
changed what `moles 128 32` answers would be a regression however good the
chemistry is.
"""

from fractions import Fraction

import pytest

from chem import formula, variants
from relations import RELATIONS


def test_a_formula_becomes_moles():
    assert variants.moles_from_mass_formula("128", "CH3OH") == "3.995 mol"
    assert variants.moles_from_mass_formula("128", "methanol") == "3.995 mol"
    assert variants.moles_from_mass_formula("36", "water") == "1.998 mol"


def test_moles_become_a_mass():
    assert variants.mass_from_moles_formula("2", "CH3OH") == "64.08 g"
    assert variants.mass_from_moles_formula("0.5", "CaCO3") == "50.04 g"


def test_ions_per_formula_unit_multiply_the_moles():
    assert variants.ions_from_moles_formula("0.2", "Ba(NO3)2") == "0.6 mol ions"
    assert variants.ions_from_moles_formula("0.1", "MgCl2") == "0.3 mol ions"


def test_ksp_from_a_solubility_and_a_formula():
    assert variants.ksp_from_solubility_formula("0.01", "Ca(OH)2") == "4e-06"
    assert variants.ksp_from_solubility_formula("0.001", "AgCl") == "1e-06"


def test_mole_fraction_from_two_masses():
    assert variants.mole_fraction_formula("128", "CH3OH", "108", "H2O") == "0.3999"
    assert variants.mole_fraction_formula("18", "H2O", "18", "CH4") == "0.471"


def test_stoichiometry_mass_from_two_formulas():
    assert variants.stoich_mass_formula("12", "Al", "2", "AlCl3") == "118.6 g"
    assert variants.stoich_mass_formula("8", "N2H4", "1", "N2") == "6.993 g"


@pytest.mark.parametrize("call,args", [
    ("moles_from_mass_formula", ("128", "propanol")),
    ("moles_from_mass_formula", ("128", "H2X")),
    ("mass_from_moles_formula", ("abc", "CH3OH")),
    ("ions_from_moles_formula", ("0.2", "CH4")),
    ("ions_from_moles_formula", ("0.2", "Na2O2")),
    ("ksp_from_solubility_formula", ("0.01", "CO2")),
    ("ksp_from_solubility_formula", ("-1", "AgCl")),
    ("mole_fraction_formula", ("128", "CH3OH", "108", "H2X")),
    ("stoich_mass_formula", ("12", "unobtainium", "2", "AlCl3")),
])
def test_refuses_rather_than_guessing(call, args):
    with pytest.raises(formula.FormulaError):
        variants.solve(call, args)


def test_the_numeric_relation_is_untouched():
    assert RELATIONS["moles"].solve(128, 32) == "4 mol"
    assert RELATIONS["mole_fraction"].solve(4, 6) == "0.4"
    assert RELATIONS["ksp_from_solubility"].solve(0.01, 1, 2) == "4e-06"
    assert RELATIONS["stoich_mass"].solve(12, 26.98, 133.34, 2) == "118.6 g"


def test_the_derived_molar_mass_is_its_own_answer():
    """The intermediate a relation would have asked for is a call of its own."""

    assert variants.molar_mass_formula("CH3OH") == "32.04 g/mol"
    assert variants.molar_mass_formula("CuSO4.5H2O") == "249.7 g/mol"


def test_a_mole_fraction_of_nothing_refuses():
    with pytest.raises(formula.FormulaError):
        variants.mole_fraction_formula("0", "H2O", "0", "H2O")


def test_molar_mass_is_exact_before_rounding():
    assert formula.molar_mass("CH3OH") == Fraction(32042, 1000)
