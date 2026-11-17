"""The words a student writes for a quantity, by register.

A relation's slot role ("volume of solution") is the precise form an author
needs; it is not what a lab report says. This module holds the vocabulary keyed
by dimension, because the words for a mass do not depend on which relation the
mass appears in, and falls back to the spec's own role words when a dimension has
no entry.

Registers are the axes a person actually varies:

  exam       the textbook register: "the molarity of the solution"
  lab        the bench register: "the mass weighed out"
  classroom  the spoken register: "grams", "mass in grams"

Structure, not register, is the holdout axis, so register words may all appear in
training: the transfer cohort measures structures never trained on, and holding
out a word as well would confound the two.
"""

REGISTERS = ("exam", "lab", "classroom")

WORDS = {
    "amount": {"exam": ("amount of substance", "number of moles"),
               "lab": ("moles on the balance sheet", "amount taken"),
               "classroom": ("moles", "amount taken")},
    "mass": {"exam": ("mass", "mass of the sample"),
             "lab": ("mass weighed out", "mass on the balance"),
             "classroom": ("grams", "mass in grams")},
    "volume": {"exam": ("volume", "volume of the solution"),
               "lab": ("volume measured out", "volume in the flask"),
               "classroom": ("litres", "volume in litres")},
    "concentration": {"exam": ("concentration", "molar concentration"),
                      "lab": ("concentration of the solution",),
                      "classroom": ("moles per litre",)},
    "pressure": {"exam": ("pressure", "pressure of the gas"),
                 "lab": ("reading on the gauge",),
                 "classroom": ("atmospheres",)},
    "temperature": {"exam": ("temperature", "temperature of the system"),
                    "lab": ("temperature on the thermometer",),
                    "classroom": ("temperature in kelvin",)},
    "energy": {"exam": ("energy", "heat energy"),
               "lab": ("heat measured",),
               "classroom": ("energy in joules",)},
    "time": {"exam": ("time interval",), "lab": ("elapsed time",),
             "classroom": ("time taken",)},
    "molar_mass": {"exam": ("molar mass", "mass of one mole"),
                   "lab": ("molar mass from the periodic table",),
                   "classroom": ("grams per mole",)},
    "count": {"exam": ("number", "count"), "lab": ("count", "number of items"),
              "classroom": ("count", "number present")},
    "ph": {"exam": ("pH",), "lab": ("pH reading",), "classroom": ("pH",)},
    "poh": {"exam": ("pOH",), "lab": ("pOH reading",), "classroom": ("pOH",)},
    "constant": {"exam": ("equilibrium constant",), "lab": ("constant for the system",),
                 "classroom": ("the constant",)},
    "efficiency": {"exam": ("efficiency", "thermal efficiency"),
                   "lab": ("efficiency of the engine",),
                   "classroom": ("efficiency",)},
    "potential": {"exam": ("cell potential", "voltage"), "lab": ("voltage reading",),
                  "classroom": ("voltage",)},
    "percent": {"exam": ("percentage",), "lab": ("percent by mass",),
                "classroom": ("percent",)},
    "enthalpy": {"exam": ("enthalpy change",), "lab": ("heat of reaction",),
                 "classroom": ("enthalpy",)},
    "entropy": {"exam": ("entropy change",), "lab": ("entropy change",),
                "classroom": ("entropy",)},
    "specific_heat": {"exam": ("specific heat capacity",),
                      "lab": ("specific heat from the table",),
                      "classroom": ("specific heat",)},
    "latent": {"exam": ("latent heat",), "lab": ("latent heat",),
               "classroom": ("latent heat",)},
    "rate": {"exam": ("rate",), "lab": ("rate measured",), "classroom": ("the rate",)},
    "density": {"exam": ("density",), "lab": ("density measured",),
                "classroom": ("density",)},
    "molality": {"exam": ("molality",), "lab": ("molality",), "classroom": ("molality",)},
    "fraction": {"exam": ("mole fraction",), "lab": ("mole fraction",),
                 "classroom": ("mole fraction",)},
    "ratio": {"exam": ("mole ratio",), "lab": ("ratio from the equation",),
              "classroom": ("ratio",)},
    "order": {"exam": ("reaction order",), "lab": ("order",), "classroom": ("order",)},
    "factor": {"exam": ("factor",), "lab": ("multiplier",), "classroom": ("factor",)},
    "angle": {"exam": ("bond angle",), "lab": ("bond angle",), "classroom": ("angle",)},
    "wavelength": {"exam": ("wavelength",), "lab": ("wavelength",),
                   "classroom": ("wavelength",)},
    "frequency": {"exam": ("frequency",), "lab": ("frequency",), "classroom": ("frequency",)},
    "absorptivity": {"exam": ("molar absorptivity",), "lab": ("absorptivity",),
                     "classroom": ("absorptivity",)},
    "length": {"exam": ("path length",), "lab": ("cuvette width",),
               "classroom": ("path length",)},
    "dimensionless": {"exam": ("absorbance",), "lab": ("absorbance reading",),
                      "classroom": ("absorbance",)},
    "charge": {"exam": ("charge passed in faradays",), "lab": ("faradays passed",),
               "classroom": ("faradays",)},
}


# Applied-context narration: how a quantity turns up inside a scenario rather than
# as a labelled slot. These are our own sentence patterns, written for this
# generator; no source text is copied or closely paraphrased. `{v}` is the value
# with its unit. A dimension with no fragment falls back to the labelled clause,
# so nothing is half-narrated.
SCENE = {
    "energy": ("the meter reads {v}", "the energy meter settles at {v}"),
    "mass": ("the balance reads {v}", "it tips the balance at {v}"),
    "volume": ("the flask is filled to {v}", "it fills a container to {v}"),
    "temperature": ("the thermometer reads {v}", "the reading settles at {v}"),
    "pressure": ("the gauge shows {v}", "the dial comes to rest at {v}"),
    "time": ("the stopwatch runs for {v}", "the clock shows {v} elapsed"),
    "concentration": ("the assay reads {v}", "the probe reports {v}"),
    "molar_mass": ("the periodic table gives {v}", "the table lists {v}"),
    "specific_heat": ("the data sheet lists {v}", "the manual quotes {v}"),
    "latent": ("the data table gives {v}", "the chart lists {v}"),
    "length": ("the ruler reads {v}", "the tape measures {v}"),
    "area": ("the panel measures {v}", "the surface covers {v}"),
    "wavelength": ("the spectrum peaks at {v}", "the fringe spacing gives {v}"),
    "frequency": ("the counter reads {v}", "the dial is set to {v}"),
    "power": ("the label says {v}", "the nameplate reads {v}"),
    "potential": ("the meter shows {v}", "the readout sits at {v}"),
    "current": ("the ammeter reads {v}", "the meter shows {v}"),
    "resistance": ("the label states {v}", "the resistor is marked {v}"),
    "enthalpy": ("the data sheet lists {v}", "the appendix gives {v}"),
    "entropy": ("the table gives {v}", "the appendix lists {v}"),
    "ratio": ("the equation fixes {v}", "the balance gives {v}"),
    "factor": ("the trial sheet lists {v}", "the run sheet records {v}"),
    "percent": ("the report quotes {v}", "the sheet shows {v}"),
    "order": ("the run sheet notes {v}", "the trial sheet records {v}"),
}


# Applied-context nouns: the apparatus and situation a row is set in. The register
# study measured that apparatus vocabulary mostly already exists in our bank but
# context nouns do not, so these are picked per dimension and per subject rather
# than rebuilt wholesale. Our own list; nothing here is a source sentence.
CONTEXT = {
    "energy": ("calorimeter", "immersion heater", "kettle"),
    "temperature": ("thermometer", "oven", "condenser"),
    "mass": ("balance", "sample", "pellet"),
    "volume": ("graduated cylinder", "flask", "tank"),
    "pressure": ("gauge", "manometer", "cylinder"),
    "time": ("stopwatch", "timer", "run"),
    "concentration": ("probe", "assay", "burette"),
    "specific_heat": ("calorimeter", "data sheet", "sample"),
    "latent": ("calorimeter", "data table", "kettle"),
    "length": ("ruler", "tape", "grating"),
    "area": ("panel", "collector", "plate"),
    "wavelength": ("spectrometer", "grating", "laser"),
    "frequency": ("counter", "oscillator", "dial"),
    "power": ("nameplate", "meter", "lamp"),
    "potential": ("voltmeter", "meter", "cell"),
    "current": ("ammeter", "meter", "circuit"),
    "resistance": ("resistor", "meter", "wire"),
    "enthalpy": ("calorimeter", "data sheet", "reaction"),
    "entropy": ("table", "cycle", "engine"),
}


def context_noun(slot, variant):
    """The apparatus or situation a slot's reading is taken in, or None."""
    nouns = CONTEXT.get(slot.dim)
    if not nouns:
        return None
    return nouns[variant % len(nouns)]


def scene_phrase(slot, value_text, variant):
    """A narration fragment for one slot, or None when its dimension has none."""
    fragments = SCENE.get(slot.dim)
    if not fragments:
        return None
    return fragments[variant % len(fragments)].format(v=value_text)


def role_words(slot, register):
    """The phrasings for one slot in one register.

    A spec role is only replaced when it is the bare dimension name, because a
    dimension word can contradict the role: `first_law`'s second slot is work,
    and calling it "energy" reads as the same quantity as the heat. Where the
    spec is precise the lexicon stays out of the way, and the variety comes from
    the clause families rather than from swapping nouns.
    """
    if slot.role != slot.dim:
        return (slot.role,)
    words = WORDS.get(slot.dim, {}).get(register, ())
    return (slot.role,) + tuple(words)


def answer_words(spec, register):
    """What the question asks for: the spec's own names for it.

    Dimension words are not merged in here either — "report the grams" is not how
    an answer is named — so the register vocabulary stays in `WORDS` for slots
    that need it rather than for every answer.
    """
    return tuple(spec.answer_words)
