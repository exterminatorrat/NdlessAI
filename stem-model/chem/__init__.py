"""A formula as a composition, a molar mass and a count of ions.

The data it reads lives in `elements.py` and is exported to the device by
`export_chem.py`, so the reference and the calculator answer from one table.
"""

from .formula import FormulaError, composition, ion_counts, molar_mass, molar_mass_mg, resolve

__all__ = [
    "FormulaError",
    "composition",
    "ion_counts",
    "molar_mass",
    "molar_mass_mg",
    "resolve",
]
