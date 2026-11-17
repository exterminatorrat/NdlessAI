"""What a relation's slots are, in the vocabulary a student would use.

`relations.py` declares parameter *names*; a name is not a role. It does not say
that `V` in `molarity(n, V)` is the volume of the whole solution rather than the
volume of water added, it does not say the unit a student writes for it, and it
does not say which slot a reader cannot tell apart from another. A generator
needs all three, so this module declares them once per relation instead of
re-deriving them from a sentence that usually does not carry them.

Three fields earn their keep:

  dim      a symbolic dimension. Two slots of one relation with the same dim are
           the pairs a reader can be left guessing about, and the ledger counts
           them per relation rather than per sentence.
  unit     what a student writes, not what the solver returns. `molarity` solves
           to "mol/L" and students write "M", "molar" and "mol per litre".
  words    the role as a person phrases it ("the volume of the solution"), which
           is what the clause library composes and what the phrasing-family
           bank has never had.

`confusable` records measured near-neighbours. It is what the contrast frames
read, so a pair that stops confusing can be seen to stop in the pair table rather
than being assumed to have stopped.

Nothing here decides whether a row is allowed: exclusion is by human
resolvability, and that is measured from the row, not declared here.
"""

from collections import namedtuple

Slot = namedtuple("Slot", "role dim unit words")
Spec = namedtuple("Spec", "name slots answer answer_words confusable same_dim")


def _slot(role, dim, unit, *words):
    return Slot(role, dim, unit, tuple(words))


def _spec(name, slots, answer, answer_words, confusable=()):
    """Build a spec and compute the same-dimension pairs once, at import."""
    slots = dict(slots)
    dims = {}
    for param, slot in slots.items():
        dims.setdefault(slot.dim, []).append(param)
    same_dim = tuple(tuple(sorted(v)) for k, v in sorted(dims.items())
                     if len(v) > 1)
    return Spec(name, slots, answer, tuple(answer_words), tuple(confusable),
                same_dim)


# The vocabulary is deliberately small and flat: a dimension a reader cannot
# distinguish from another is the same string here, which is the point.
SPECS = {
    "atoms_from_mass": _spec(
        "atoms_from_mass",
        [("mass", _slot("mass of the sample", "mass", "g", "g of the sample")),
         ("molar_mass", _slot("molar mass", "molar_mass", "g/mol",
                              "g/mol", "grams per mole"))],
        _slot("number of atoms", "count", "atoms"),
        ("number of atoms", "atom count"),
    ),
    "avg_rate": _spec(
        "avg_rate",
        [("change", _slot("change in concentration", "concentration", "M")),
         ("time", _slot("time interval", "time", "s"))],
        _slot("average rate", "rate", "M/s"),
        ("average rate", "rate of reaction", "rate of the reaction"),
        confusable=("rate_factor", "half_life"),
    ),
    "beer_lambert": _spec(
        "beer_lambert",
        [("A", _slot("absorbance", "dimensionless", "", "absorbance")),
         ("epsilon", _slot("molar absorptivity", "absorptivity", "L/(mol cm)")),
         ("l", _slot("path length", "length", "cm"))],
        _slot("concentration", "concentration", "mol/L"),
        ("concentration", "molar concentration"),
        confusable=("molarity", "dilution"),
    ),
    "boyles": _spec(
        "boyles",
        [("P1", _slot("initial pressure", "pressure", "atm")),
         ("V1", _slot("initial volume", "volume", "L")),
         ("V2", _slot("final volume", "volume", "L"))],
        _slot("final pressure", "pressure", "atm"),
        ("pressure after compression", "resulting pressure"),
        confusable=("charles", "combined_gas", "gas_mixture_pressure"),
    ),
    "buffer_ph": _spec(
        "buffer_ph",
        [("pka", _slot("pKa of the acid", "ph", "")),
         ("base", _slot("concentration of the base form", "concentration", "M")),
         ("acid", _slot("concentration of the acid form", "concentration", "M"))],
        _slot("pH", "ph", ""),
        ("pH", "pH of the buffer"),
        confusable=("ph", "ph_from_poh", "poh"),
    ),
    "calorimetry_c": _spec(
        "calorimetry_c",
        [("q", _slot("heat absorbed", "energy", "J")),
         ("m", _slot("mass of the sample", "mass", "g")),
         ("dT", _slot("temperature change", "temperature", "C"))],
        _slot("specific heat", "specific_heat", "J/(g C)"),
        ("specific heat", "specific heat capacity"),
        confusable=("calorimetry_mass", "latent_heat", "heat_reaction"),
    ),
    "calorimetry_mass": _spec(
        "calorimetry_mass",
        [("q", _slot("heat absorbed", "energy", "J")),
         ("c", _slot("specific heat", "specific_heat", "J/(g C)")),
         ("dT", _slot("temperature change", "temperature", "C"))],
        _slot("mass", "mass", "g"),
        ("mass", "mass of the sample"),
        confusable=("calorimetry_c", "moles", "mass_from_molarity"),
    ),
    "carnot": _spec(
        "carnot",
        [("t_hot", _slot("hot reservoir temperature", "temperature", "K")),
         ("t_cold", _slot("cold reservoir temperature", "temperature", "K"))],
        _slot("efficiency", "efficiency", "%"),
        ("efficiency", "efficiency as a percent"),
        confusable=("carnot_efficiency",),
    ),
    "carnot_efficiency": _spec(
        "carnot_efficiency",
        [("Tc", _slot("cold reservoir temperature", "temperature", "K")),
         ("Th", _slot("hot reservoir temperature", "temperature", "K"))],
        _slot("efficiency", "efficiency", ""),
        ("efficiency", "efficiency as a decimal"),
        confusable=("carnot",),
    ),
    "cell_potential": _spec(
        "cell_potential",
        [("e_cathode", _slot("cathode potential", "potential", "V")),
         ("e_anode", _slot("anode potential", "potential", "V"))],
        _slot("cell potential", "potential", "V"),
        ("cell potential", "cell voltage", "EMF of the cell"),
        confusable=("gibbs",),
    ),
    "charles": _spec(
        "charles",
        [("V1", _slot("initial volume", "volume", "L")),
         ("T1", _slot("initial temperature", "temperature", "K")),
         ("T2", _slot("final temperature", "temperature", "K"))],
        _slot("final volume", "volume", "L"),
        ("final volume", "new volume", "volume after the change"),
        confusable=("boyles", "combined_gas", "gas_p"),
    ),
    "combined_gas": _spec(
        "combined_gas",
        [("P1", _slot("initial pressure", "pressure", "atm")),
         ("V1", _slot("initial volume", "volume", "L")),
         ("T1", _slot("initial temperature", "temperature", "K")),
         ("P2", _slot("final pressure", "pressure", "atm")),
         ("T2", _slot("final temperature", "temperature", "K"))],
        _slot("final volume", "volume", "L"),
        ("final volume", "new volume"),
        confusable=("boyles", "charles", "gas_p"),
    ),
    "density": _spec(
        "density",
        [("m", _slot("mass", "mass", "g")),
         ("V", _slot("volume", "volume", "mL"))],
        _slot("density", "density", "g/mL"),
        ("density",),
        confusable=("molarity",),
    ),
    "dilution": _spec(
        "dilution",
        [("c1", _slot("stock concentration", "concentration", "M")),
         ("v1", _slot("volume of stock taken", "volume", "mL")),
         ("v2", _slot("final volume", "volume", "mL"))],
        _slot("final concentration", "concentration", "M"),
        ("final concentration", "new molarity", "concentration after diluting"),
        confusable=("dilution_v1", "molarity", "mixing_concentration"),
    ),
    "dilution_v1": _spec(
        "dilution_v1",
        [("m1", _slot("stock concentration", "concentration", "M")),
         ("m2", _slot("target concentration", "concentration", "M")),
         ("v2", _slot("final volume", "volume", "mL"))],
        _slot("volume of stock", "volume", "mL"),
        ("volume of stock needed", "volume of the stock solution"),
        confusable=("dilution", "mixing_concentration", "molarity"),
    ),
    "faraday_deposit": _spec(
        "faraday_deposit",
        [("faradays", _slot("faradays passed", "charge", "F")),
         ("electrons_per_ion", _slot("electrons per ion", "count", "")),
         ("molar_mass", _slot("molar mass", "molar_mass", "g/mol"))],
        _slot("mass deposited", "mass", "g"),
        ("mass deposited", "grams plated out"),
        confusable=("mass_from_molarity", "heat_reaction"),
    ),
    "first_law": _spec(
        "first_law",
        [("q", _slot("heat added", "energy", "J")),
         ("w", _slot("work done", "energy", "J"))],
        _slot("change in internal energy", "energy", "J"),
        ("change in internal energy", "dU"),
        confusable=("heat_reaction", "latent_heat"),
    ),
    "gas_mixture_pressure": _spec(
        "gas_mixture_pressure",
        [("P_i", _slot("initial pressure of the gas", "pressure", "atm")),
         ("V_i", _slot("initial volume", "volume", "L")),
         ("V_f", _slot("final volume", "volume", "L"))],
        _slot("final pressure", "pressure", "atm"),
        ("final total pressure", "pressure of the combined gases"),
        confusable=("boyles", "partial_pressure"),
    ),
    "gas_p": _spec(
        "gas_p",
        [("n", _slot("amount of gas", "amount", "mol")),
         ("T", _slot("temperature", "temperature", "K")),
         ("V", _slot("volume", "volume", "L"))],
        _slot("pressure", "pressure", "atm"),
        ("pressure", "pressure of the gas"),
        confusable=("boyles", "combined_gas"),
    ),
    "gibbs": _spec(
        "gibbs",
        [("dH", _slot("enthalpy change", "enthalpy", "kJ")),
         ("T", _slot("temperature", "temperature", "K")),
         ("dS", _slot("entropy change", "entropy", "J/K"))],
        _slot("Gibbs free energy change", "energy", "kJ"),
        ("Gibbs free energy change", "dG"),
        confusable=("cell_potential", "heat_reaction"),
    ),
    "h_from_ph": _spec(
        "h_from_ph",
        [("ph", _slot("pH", "ph", ""))],
        _slot("hydrogen ion concentration", "concentration", "mol/L"),
        ("hydrogen ion concentration", "[H+]"),
        confusable=("ph", "ph_from_poh", "poh"),
    ),
    "heat_reaction": _spec(
        "heat_reaction",
        [("moles", _slot("amount reacted", "amount", "mol")),
         ("enthalpy", _slot("enthalpy of reaction", "enthalpy", "kJ/mol"))],
        _slot("heat released", "energy", "kJ"),
        ("heat released", "energy given off"),
        confusable=("first_law", "latent_heat", "calorimetry_c"),
    ),
    "hydrate_water": _spec(
        "hydrate_water",
        [("mass_hydrate", _slot("mass of the hydrate", "mass", "g")),
         ("mass_anhydrous", _slot("mass after heating", "mass", "g")),
         ("mm_anhydrous", _slot("molar mass of the anhydrous salt", "molar_mass", "g/mol")),
         ("mm_water", _slot("molar mass of water", "molar_mass", "g/mol"))],
        _slot("water of hydration", "ratio", ""),
        ("water of hydration", "waters of crystallization"),
        confusable=("mass_percent",),
    ),
    "kc_from_conc": _spec(
        "kc_from_conc",
        [("product", _slot("equilibrium concentration of product", "concentration", "M")),
         ("reactant", _slot("equilibrium concentration of reactant", "concentration", "M")),
         ("coefficient", _slot("coefficient on the reactant", "count", ""))],
        _slot("equilibrium constant", "constant", ""),
        ("equilibrium constant", "Kc"),
        confusable=("ksp_from_solubility",),
    ),
    "ksp_from_solubility": _spec(
        "ksp_from_solubility",
        [("s", _slot("molar solubility", "concentration", "M")),
         ("cation_count", _slot("cations per formula unit", "count", "")),
         ("anion_count", _slot("anions per formula unit", "count", ""))],
        _slot("solubility product", "constant", ""),
        ("solubility product", "Ksp"),
        confusable=("kc_from_conc", "molarity"),
    ),
    "latent_heat": _spec(
        "latent_heat",
        [("m", _slot("mass", "mass", "g")),
         ("L", _slot("latent heat", "latent", "J/g"))],
        _slot("heat required", "energy", "J"),
        ("heat required", "energy absorbed"),
        confusable=("calorimetry_c", "heat_reaction", "first_law"),
    ),
    "limiting_moles": _spec(
        "limiting_moles",
        [("mass_a", _slot("mass of reactant A", "mass", "g")),
         ("mm_a", _slot("molar mass of A", "molar_mass", "g/mol")),
         ("mass_b", _slot("mass of reactant B", "mass", "g")),
         ("mm_b", _slot("molar mass of B", "molar_mass", "g/mol"))],
        _slot("limiting amount", "amount", "mol"),
        ("limiting amount", "limiting reactant in moles"),
        confusable=("stoichiometry", "moles"),
    ),
    "mass_from_molarity": _spec(
        "mass_from_molarity",
        [("c", _slot("concentration", "concentration", "M")),
         ("V", _slot("volume of solution", "volume", "L")),
         ("molar_mass", _slot("molar mass", "molar_mass", "g/mol"))],
        _slot("mass of solute", "mass", "g"),
        ("mass of solute", "grams of solute"),
        confusable=("molarity", "moles", "calorimetry_mass"),
    ),
    "mass_from_percent_composition": _spec(
        "mass_from_percent_composition",
        [("percent", _slot("percent by mass", "percent", "%")),
         ("total_mass", _slot("total mass", "mass", "g"))],
        _slot("mass of the element", "mass", "g"),
        ("mass of the element", "grams of that element"),
        confusable=("mass_percent",),
    ),
    "mass_percent": _spec(
        "mass_percent",
        [("solute", _slot("mass of solute", "mass", "g")),
         ("solution", _slot("mass of solution", "mass", "g"))],
        _slot("percent by mass", "percent", "%"),
        ("percent by mass", "mass percent"),
        confusable=("mass_from_percent_composition", "percent_yield", "hydrate_water"),
    ),
    "mixing_concentration": _spec(
        "mixing_concentration",
        [("v1", _slot("volume of the first solution", "volume", "mL")),
         ("c1", _slot("concentration of the first solution", "concentration", "M")),
         ("v2", _slot("volume of the second solution", "volume", "mL")),
         ("c2", _slot("concentration of the second solution", "concentration", "M"))],
        _slot("final concentration", "concentration", "M"),
        ("final molarity", "molarity of the mixed solution"),
        confusable=("dilution", "dilution_v1", "molarity"),
    ),
    "mixture_final_temperature": _spec(
        "mixture_final_temperature",
        [("m1", _slot("mass of the first sample", "mass", "g")),
         ("T1", _slot("temperature of the first sample", "temperature", "K")),
         ("m2", _slot("mass of the second sample", "mass", "g")),
         ("T2", _slot("temperature of the second sample", "temperature", "K"))],
        _slot("final temperature", "temperature", "K"),
        ("final temperature", "equilibrium temperature"),
        confusable=("calorimetry_c",),
    ),
    "molality": _spec(
        "molality",
        [("n", _slot("amount of solute", "amount", "mol")),
         ("kg", _slot("mass of solvent", "mass", "kg"))],
        _slot("molality", "molality", "mol/kg"),
        ("molality",),
        confusable=("molarity",),
    ),
    "molarity": _spec(
        "molarity",
        [("n", _slot("amount of solute", "amount", "mol")),
         ("V", _slot("volume of solution", "volume", "L"))],
        _slot("molarity", "concentration", "mol/L"),
        ("molarity", "molar concentration", "concentration in mol per liter"),
        confusable=("mass_from_molarity", "dilution", "mixing_concentration"),
    ),
    "mole_fraction": _spec(
        "mole_fraction",
        [("na", _slot("amount of A", "amount", "mol")),
         ("nb", _slot("amount of B", "amount", "mol"))],
        _slot("mole fraction of A", "fraction", ""),
        ("mole fraction of A", "mole fraction"),
        confusable=("partial_pressure",),
    ),
    "moles": _spec(
        "moles",
        [("mass", _slot("mass", "mass", "g")),
         ("molar_mass", _slot("molar mass", "molar_mass", "g/mol"))],
        _slot("amount in moles", "amount", "mol"),
        ("amount in moles", "number of moles"),
        confusable=("atoms_from_mass", "mass_from_molarity", "stoichiometry"),
    ),
    "partial_pressure": _spec(
        "partial_pressure",
        [("n_i", _slot("amount of the gas", "amount", "mol")),
         ("n_total", _slot("total amount of gas", "amount", "mol")),
         ("P_total", _slot("total pressure", "pressure", "atm"))],
        _slot("partial pressure", "pressure", "atm"),
        ("partial pressure", "pressure of that gas"),
        confusable=("gas_mixture_pressure", "mole_fraction", "gas_p"),
    ),
    "percent_dissociation": _spec(
        "percent_dissociation",
        [("c0", _slot("initial concentration", "concentration", "M")),
         ("Ka", _slot("acid dissociation constant", "constant", ""))],
        _slot("percent ionization", "percent", "%"),
        ("percent ionization", "percent dissociation", "percent ionized"),
        confusable=("ph", "h_from_ph", "buffer_ph"),
    ),
    "percent_yield": _spec(
        "percent_yield",
        [("actual", _slot("actual yield", "mass", "g")),
         ("theoretical", _slot("theoretical yield", "mass", "g"))],
        _slot("percent yield", "percent", "%"),
        ("percent yield", "percentage yield"),
        confusable=("mass_percent", "percent_change", "percent_error"),
    ),
    "ph": _spec(
        "ph",
        [("H", _slot("hydrogen ion concentration", "concentration", "mol/L"))],
        _slot("pH", "ph", ""),
        ("pH",),
        confusable=("h_from_ph", "poh", "ph_from_poh"),
    ),
    "ph_from_poh": _spec(
        "ph_from_poh",
        [("poh", _slot("pOH", "poh", ""))],
        _slot("pH", "ph", ""),
        ("pH",),
        confusable=("ph", "poh", "h_from_ph"),
    ),
    "ph_of_water": _spec(
        "ph_of_water",
        [("pkw", _slot("pKw", "poh", ""))],
        _slot("pH of pure water", "ph", ""),
        ("pH of pure water", "neutral pH"),
        confusable=("ph",),
    ),
    "poh": _spec(
        "poh",
        [("OH", _slot("hydroxide concentration", "concentration", "mol/L"))],
        _slot("pOH", "poh", ""),
        ("pOH",),
        confusable=("ph", "ph_from_poh", "h_from_ph"),
    ),
    "rate_factor": _spec(
        "rate_factor",
        [("f1", _slot("first factor", "factor", "")),
         ("o1", _slot("order in the first factor", "order", "")),
         ("f2", _slot("second factor", "factor", "")),
         ("o2", _slot("order in the second factor", "order", ""))],
        _slot("factor by which the rate changes", "factor", ""),
        ("factor by which the rate changes", "rate multiplier"),
        confusable=("avg_rate",),
    ),
    "stoich_mass": _spec(
        "stoich_mass",
        [("mass", _slot("mass of the given substance", "mass", "g")),
         ("mm_given", _slot("molar mass of the given substance", "molar_mass", "g/mol")),
         ("mm_product", _slot("molar mass of the wanted substance", "molar_mass", "g/mol")),
         ("ratio", _slot("mole ratio", "ratio", ""))],
        _slot("mass of the wanted substance", "mass", "g"),
        ("mass of the wanted substance", "grams produced"),
        confusable=("stoichiometry", "stoich_volume_gas", "limiting_moles"),
    ),
    "stoich_volume_gas": _spec(
        "stoich_volume_gas",
        [("V_given", _slot("volume of the given gas", "volume", "L")),
         ("ratio", _slot("mole ratio", "ratio", ""))],
        _slot("volume of the wanted gas", "volume", "L"),
        ("volume of the wanted gas", "liters produced"),
        confusable=("stoich_mass", "stoichiometry"),
    ),
    "stoichiometry": _spec(
        "stoichiometry",
        [("moles_given", _slot("amount of the given substance", "amount", "mol")),
         ("ratio", _slot("mole ratio", "ratio", ""))],
        _slot("amount of product", "amount", "mol"),
        ("amount of product", "moles of product"),
        confusable=("stoich_mass", "limiting_moles", "moles"),
    ),
    "vsepr_bond_angle": _spec(
        "vsepr_bond_angle",
        [("electron_domains", _slot("electron domains", "count", "")),
         ("lone_pairs", _slot("lone pairs", "count", ""))],
        _slot("bond angle", "angle", "degrees"),
        ("bond angle", "angle between the bonds"),
        confusable=(),
    ),
    "wavelength_from_frequency": _spec(
        "wavelength_from_frequency",
        [("frequency_thz", _slot("frequency", "frequency", "THz"))],
        _slot("wavelength", "wavelength", "m"),
        ("wavelength",),
        confusable=(),
    ),
}

# Physics, heat transfer and electromagnetic waves. These are the two clusters the
# sourcing pass measured as answerable with one numeric step (69% and 60% of their
# textbook pools) rather than needing a figure or a derivation, so they are where a
# real question pool exists for this catalogue to answer.
PHYSICS = {
    "specific_heat": _spec(
        "specific_heat",
        [("m", _slot("mass of the sample", "mass", "g")),
         ("c", _slot("specific heat", "specific_heat", "J/(g C)")),
         ("dT", _slot("temperature change", "temperature", "C"))],
        _slot("heat transferred", "energy", "J"),
        ("heat transferred", "heat added", "energy needed"),
        confusable=("calorimetry_c", "calorimetry_mass", "gas_heat_cp"),
    ),
    "conduction_rate": _spec(
        "conduction_rate",
        [("k", _slot("thermal conductivity", "conductivity", "W/(m K)")),
         ("A", _slot("area of the slab", "area", "m^2")),
         ("dT", _slot("temperature difference", "temperature", "K")),
         ("d", _slot("thickness of the slab", "length", "m"))],
        _slot("rate of heat flow", "power", "W"),
        ("rate of heat flow", "power conducted", "heat current"),
        confusable=("radiation_power", "specific_heat"),
    ),
    "radiation_power": _spec(
        "radiation_power",
        [("T", _slot("absolute temperature", "temperature", "K")),
         ("area", _slot("surface area", "area", "m^2"))],
        _slot("radiated power", "power", "W"),
        ("radiated power", "power emitted", "rate of radiation"),
        confusable=("conduction_rate",),
    ),
    "gas_heat_cp": _spec(
        "gas_heat_cp",
        [("n", _slot("amount of gas", "amount", "mol")),
         ("cp", _slot("molar heat capacity at constant pressure",
                      "heat_capacity", "J/(mol K)")),
         ("dT", _slot("temperature change", "temperature", "K"))],
        _slot("heat absorbed", "energy", "J"),
        ("heat absorbed", "energy supplied", "heat needed"),
        confusable=("specific_heat", "entropy_change"),
    ),
    "entropy_change": _spec(
        "entropy_change",
        [("q", _slot("heat added reversibly", "energy", "J")),
         ("T", _slot("absolute temperature", "temperature", "K"))],
        _slot("entropy change", "entropy", "J/K"),
        ("entropy change", "change in entropy"),
        confusable=("first_law", "gas_heat_cp"),
    ),
    "wave_speed": _spec(
        "wave_speed",
        [("f", _slot("frequency", "frequency", "Hz")),
         ("lam", _slot("wavelength", "wavelength", "m"))],
        _slot("wave speed", "speed", "m/s"),
        ("wave speed", "speed of the wave"),
        confusable=("wavelength_from_frequency", "standing_wave_freq"),
    ),
    "standing_wave_freq": _spec(
        "standing_wave_freq",
        [("n", _slot("harmonic number", "count", "")),
         ("v", _slot("wave speed", "speed", "m/s")),
         ("L", _slot("length of the string", "length", "m"))],
        _slot("resonant frequency", "frequency", "Hz"),
        ("resonant frequency", "frequency of the harmonic"),
        confusable=("wave_speed", "beat_freq", "doppler_freq"),
    ),
    "double_slit_fringe": _spec(
        "double_slit_fringe",
        [("m", _slot("fringe order", "count", "")),
         ("lam", _slot("wavelength of the light", "wavelength", "nm")),
         ("L", _slot("distance to the screen", "length", "m")),
         ("d", _slot("slit separation", "length", "mm"))],
        _slot("fringe separation", "length", "mm"),
        ("fringe separation", "distance between fringes"),
        confusable=("wave_speed", "standing_wave_freq"),
    ),
    "doppler_freq": _spec(
        "doppler_freq",
        [("f", _slot("emitted frequency", "frequency", "Hz")),
         ("v", _slot("speed of sound", "speed", "m/s")),
         ("vs", _slot("speed of the source", "speed", "m/s"))],
        _slot("observed frequency", "frequency", "Hz"),
        ("observed frequency", "frequency the listener hears"),
        confusable=("beat_freq", "standing_wave_freq"),
    ),
    "beat_freq": _spec(
        "beat_freq",
        [("f1", _slot("first frequency", "frequency", "Hz")),
         ("f2", _slot("second frequency", "frequency", "Hz"))],
        _slot("beat frequency", "frequency", "Hz"),
        ("beat frequency", "beat rate"),
        confusable=("doppler_freq", "standing_wave_freq"),
    ),
}

SPECS.update(PHYSICS)

# Provenance per relation: which registers the spec's rows are written in, and
# which entry of SOURCES the structural idea came from. The licensing rule
# (Harry, 2026-11-17) is that non-commercial material is read for structure and
# register only — no sentence is copied, every row is authored from a spec, and no
# row reproduces six consecutive words of a source — so this note is what makes
# that claim auditable later instead of a promise.
SOURCES = {
    "in-repo": "authored in this repository from the relation's own contract; "
               "no outside source",
    "physics-pool": "structural patterns only from the sourced physics pool "
                    "(OpenStax College Physics 2e, CC BY-NC-SA 4.0; CK-12, CC "
                    "BY-NC). No sentence was read into a fixture, and no phrase "
                    "is reproduced",
    "chemistry-pool": "structural patterns only from the sourced chemistry pool "
                      "(OpenStax Chemistry 2e, CC BY-NC-SA 4.0). No sentence was "
                      "read into a fixture, and no phrase is reproduced",
}

DEFAULT_PROVENANCE = ("labelled givens, imperative ask", "in-repo")

APPLIED = "applied context (device and bench narration), worked-example prose"

PROVENANCE = {name: (APPLIED, "physics-pool") for name in PHYSICS}
PROVENANCE.update({
    name: (APPLIED, "chemistry-pool")
    for name in ("latent_heat", "heat_reaction", "first_law",
                 "mixture_final_temperature", "calorimetry_c", "calorimetry_mass")
})
PROVENANCE["wavelength_from_frequency"] = (
    "applied context (instrument dial and spectrum reading), worked-example prose",
    "physics-pool",
)
