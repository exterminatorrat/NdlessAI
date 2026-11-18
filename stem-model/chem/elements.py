"""Reference chemistry data: atomic weights, ion charges, and measured names.

This module is data, not logic. `formula.py` parses a formula with it, and
`export_chem.py` writes the same data into `runtime/chemtable.h`, so the device
and the reference answer from one table rather than two copies that can drift.

Three decisions worth stating.

**Weights are integer thousandths of a gram per mole.** CH3OH is
12*1000 + 4*1008 + 15999 = 32042, rendered 32.04 g/mol at four significant
digits by the same exact-rational renderer every other relation uses. There is
no float anywhere in either language, so the device and the reference print the
same digits for the same formula.

**Only ions with an unambiguous charge are tabulated.** A metal whose charge a
formula can settle (Fe(NO3)3 is +3 because nitrate is -1) is inferred from the
composition and the anion, so no table entry can be wrong about it. A metal
whose charge is fixed by chemistry (Na is +1, Ba is +2) is recorded, which is
what refuses Na2O2: two +1 cations against two 2- ions do not balance, so the
formula is not the peroxide it looks like and the caller is told so.

**Names are the measured ones, and nothing else.** The list covers every
compound name that appears in the committed corpus or the local AP Chemistry
bank without a formula beside it — 33 names across 8,784 rows. A name that is
not on it is refused, because a guessed formula is a wrong molar mass with a
reason attached.
"""

# IUPAC abridged standard atomic weights, g/mol to three decimals, as integer
# thousandths. Elements with no stable isotope carry their mass number.
ATOMIC_WEIGHTS = {
    "H": 1008, "He": 4003, "Li": 6940, "Be": 9012, "B": 10810, "C": 12011,
    "N": 14007, "O": 15999, "F": 18998, "Ne": 20180, "Na": 22990, "Mg": 24305,
    "Al": 26982, "Si": 28085, "P": 30974, "S": 32060, "Cl": 35450, "Ar": 39948,
    "K": 39098, "Ca": 40078, "Sc": 44956, "Ti": 47867, "V": 50942, "Cr": 51996,
    "Mn": 54938, "Fe": 55845, "Co": 58933, "Ni": 58693, "Cu": 63546,
    "Zn": 65380, "Ga": 69723, "Ge": 72630, "As": 74922, "Se": 78971,
    "Br": 79904, "Kr": 83798, "Rb": 85468, "Sr": 87620, "Y": 88906,
    "Zr": 91224, "Nb": 92906, "Mo": 95950, "Tc": 98000, "Ru": 101070,
    "Rh": 102910, "Pd": 106420, "Ag": 107870, "Cd": 112410, "In": 114820,
    "Sn": 118710, "Sb": 121760, "Te": 127600, "I": 126900, "Xe": 131290,
    "Cs": 132910, "Ba": 137330, "La": 138910, "Ce": 140120, "Pr": 140910,
    "Nd": 144240, "Pm": 145000, "Sm": 150360, "Eu": 151960, "Gd": 157250,
    "Tb": 158930, "Dy": 162500, "Ho": 164930, "Er": 167260, "Tm": 168930,
    "Yb": 173050, "Lu": 174970, "Hf": 178490, "Ta": 180950, "W": 183840,
    "Re": 186210, "Os": 190230, "Ir": 192220, "Pt": 195080, "Au": 196970,
    "Hg": 200590, "Tl": 204380, "Pb": 207200, "Bi": 208980, "Po": 209000,
    "At": 210000, "Rn": 222000, "Fr": 223000, "Ra": 226000, "Ac": 227000,
    "Th": 232040, "Pa": 231040, "U": 238030, "Np": 237000, "Pu": 244000,
    "Am": 243000, "Cm": 247000, "Bk": 247000, "Cf": 251000, "Es": 252000,
    "Fm": 257000, "Md": 258000, "No": 259000, "Lr": 266000, "Rf": 267000,
    "Db": 268000, "Sg": 269000, "Bh": 270000, "Hs": 269000, "Mt": 278000,
    "Ds": 281000, "Rg": 282000, "Cn": 285000, "Nh": 286000, "Fl": 289000,
    "Mc": 290000, "Lv": 293000, "Ts": 294000, "Og": 294000,
}

# A formula whose leading element is not on this list is not an ionic compound
# as far as this module is concerned, and ion counts are refused. Molecular
# formulas such as CH4 and ClF3 therefore never reach the charge arithmetic.
METALS = """
Li Be Na Mg Al K Ca Sc Ti V Cr Mn Fe Co Ni Cu Zn Ga Rb Sr Y Zr Nb Mo Tc Ru
Rh Pd Ag Cd In Sn Cs Ba La Ce Pr Nd Pm Sm Eu Gd Tb Dy Ho Er Tm Yb Lu Hf Ta W
Re Os Ir Pt Au Hg Tl Pb Bi Po Fr Ra Ac Th Pa U Np Pu Am Cm Bk Cf Es Fm Md No
Lr Rf Db Sg Bh Hs Mt Ds Rg Cn Nh Fl Mc Lv
""".split()

# The charge of a metal the periodic table settles on its own. A metal absent
# from this table has its charge inferred from the anion and the composition,
# which is the only honest way to read Fe(NO3)2 against Fe(NO3)3.
FIXED_CATION_CHARGES = {
    "Li": 1, "Na": 1, "K": 1, "Rb": 1, "Cs": 1, "Fr": 1,
    "Be": 2, "Mg": 2, "Ca": 2, "Sr": 2, "Ba": 2, "Ra": 2,
    "Al": 3, "Zn": 2, "Ag": 1,
}

# Polyatomic ions by their written formula, with the magnitude of the charge.
# The ammonium ions are cations and are listed apart because their position in
# the formula, not their sign, is what the caller needs.
POLYATOMIC_ANIONS = {
    "NO3": 1, "NO2": 1, "OH": 1, "SO4": 2, "SO3": 2, "CO3": 2, "PO4": 3,
    "CN": 1, "SCN": 1, "MnO4": 1, "C2H3O2": 1, "ClO": 1, "ClO2": 1,
    "ClO3": 1, "ClO4": 1, "CrO4": 2, "Cr2O7": 2, "HCO3": 1, "HSO4": 1,
    "HSO3": 1, "H2PO4": 1, "HPO4": 2, "C2O4": 2, "IO3": 1, "BrO3": 1,
}

POLYATOMIC_CATIONS = {"NH4": 1, "H3O": 1}

# Monatomic anions. A metal plus one of these is the whole formula, so the
# charge cannot be a guess: the element's group fixes it.
MONATOMIC_ANION_CHARGES = {
    "F": 1, "Cl": 1, "Br": 1, "I": 1, "O": 2, "S": 2, "N": 3, "P": 3,
    "H": 1, "Se": 2, "Te": 2,
}

# The measured name table. Each name maps to one molecular formula, so an
# isomer or a hydrate that the formula cannot settle is absent on purpose.
NAMES = {
    "acetic acid": "CH3COOH",
    "ammonia": "NH3",
    "benzene": "C6H6",
    "benzoic acid": "C6H5COOH",
    "butane": "C4H10",
    "calcium carbonate": "CaCO3",
    "carbon dioxide": "CO2",
    "carbon monoxide": "CO",
    "ethane": "C2H6",
    "ethanoic acid": "CH3COOH",
    "ethanol": "C2H5OH",
    "glucose": "C6H12O6",
    "glycerol": "C3H8O3",
    "heptane": "C7H16",
    "hexane": "C6H14",
    "hydrochloric acid": "HCl",
    "hydrogen peroxide": "H2O2",
    "lactic acid": "C3H6O3",
    "methane": "CH4",
    "methanol": "CH3OH",
    "nitric acid": "HNO3",
    "nitrous acid": "HNO2",
    "nonane": "C9H20",
    "octane": "C8H18",
    "oxalic acid": "H2C2O4",
    "ozone": "O3",
    "pentane": "C5H12",
    "propanoic acid": "C2H5COOH",
    "sodium chloride": "NaCl",
    "sodium hydroxide": "NaOH",
    "sucrose": "C12H22O11",
    "sulfuric acid": "H2SO4",
    "water": "H2O",
}
