"""The formula parser against the formulas a chemistry student actually writes.

The refusals are the point. A molar mass computed from a misread formula is a
wrong answer with no trace of the mistake, so every case here that raises is a
case the app must decline rather than answer.
"""

from fractions import Fraction

import pytest

from chem import formula


def mm(text):
    return formula.molar_mass(text)


def test_composition_reads_groups_and_counts():
    assert formula.composition("H2O") == {"H": 2, "O": 1}
    assert formula.composition("Ba(NO3)2") == {"Ba": 1, "N": 2, "O": 6}
    assert formula.composition("Al2(SO4)3") == {"Al": 2, "S": 3, "O": 12}
    assert formula.composition("Ca3(PO4)2") == {"Ca": 3, "P": 2, "O": 8}


def test_composition_reads_hydrates():
    assert formula.composition("CuSO4.5H2O") == {
        "Cu": 1, "S": 1, "O": 9, "H": 10}
    assert formula.composition("CuSO4·5H2O") == formula.composition("CuSO4.5H2O")
    assert formula.composition("Na2CO3.10H2O") == {
        "Na": 2, "C": 1, "O": 13, "H": 20}


def test_composition_accepts_unicode_subscripts():
    assert formula.composition("H₂O") == {"H": 2, "O": 1}
    assert formula.composition("Ba(NO₃)₂") == {"Ba": 1, "N": 2, "O": 6}


def test_molar_mass_is_the_standard_weights():
    assert mm("H2O") == Fraction(18015, 1000)
    assert mm("CH3OH") == Fraction(32042, 1000)
    assert mm("NaCl") == Fraction(58440, 1000)
    assert mm("CaCO3") == Fraction(100086, 1000)
    assert mm("CuSO4.5H2O") == Fraction(249677, 1000)
    assert mm("C6H12O6") == Fraction(180156, 1000)


def test_molar_mass_reads_a_measured_name():
    assert mm("methanol") == mm("CH3OH")
    assert mm("water") == mm("H2O")
    assert mm("carbon_dioxide") == mm("CO2")
    assert mm("acetic-acid") == mm("CH3COOH")
    assert mm("Sodium Chloride") == mm("NaCl")


def test_a_charge_does_not_change_the_mass():
    assert mm("Ba2+") == mm("Ba")
    assert mm("Ba(NO3)2 2+") == mm("Ba(NO3)2")


@pytest.mark.parametrize("text", [
    "H2X",          # an element symbol that does not exist
    "Xx2",
    "Ca(OH2",       # unbalanced
    "CaOH)2",
    "H0",           # a subscript of zero
    "2H2O",         # a coefficient is not a formula
    "NaCl(",        # trailing garbage
    "",             # nothing at all
    "unobtainium",  # not a formula and not a measured name
    "propanol",     # an isomer the formula alone cannot settle
    "copper(II) sulfate pentahydrate",  # a hydrate with no formula on the table
])
def test_refuses_rather_than_guesses(text):
    with pytest.raises(formula.FormulaError):
        formula.composition(text)


def test_refuses_a_formula_past_what_the_build_holds():
    with pytest.raises(formula.FormulaError):
        formula.composition("C" + "999" + "H" * 10)


def test_ion_counts_read_the_formula_unit():
    assert formula.ion_counts("NaCl") == (1, 1)
    assert formula.ion_counts("MgCl2") == (1, 2)
    assert formula.ion_counts("Ca(OH)2") == (1, 2)
    assert formula.ion_counts("Ba(NO3)2") == (1, 2)
    assert formula.ion_counts("K2CO3") == (2, 1)
    assert formula.ion_counts("Al2(SO4)3") == (2, 3)
    assert formula.ion_counts("Cu3(PO4)2") == (3, 2)
    assert formula.ion_counts("KMnO4") == (1, 1)
    assert formula.ion_counts("NaC2H3O2") == (1, 1)
    assert formula.ion_counts("NH4Cl") == (1, 1)
    assert formula.ion_counts("(NH4)2SO4") == (2, 1)
    assert formula.ion_counts("Fe(NO3)3") == (1, 3)
    assert formula.ion_counts("CaF2") == (1, 2)


@pytest.mark.parametrize("text", [
    "CH4",              # molecular, not ionic
    "CO2",
    "ClF3",
    "Na2O2",            # peroxide: the charges do not balance as written
    "NaCl2",
    "CuSO4.5H2O",       # a hydrate is not one formula unit
    "Ba2+",             # an ion is not a formula unit
    "Na",               # an element is not a formula unit
    "Cs2S2O3",          # an anion this table does not carry
])
def test_ion_counts_refuse(text):
    with pytest.raises(formula.FormulaError):
        formula.ion_counts(text)


def test_resolve_returns_the_formula_it_understood():
    assert formula.resolve("methanol")[0] == "CH3OH"
    assert formula.resolve("Ba(NO3)2")[0] == "Ba(NO3)2"
