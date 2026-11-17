"""Compose a chemistry sentence from clauses, and refuse what a reader cannot resolve.

`solutions.py`-style template lists make one sentence per entry, so a relation
with thirteen entries has thirteen sentence structures however many thousand
rows are drawn from them. Here a sentence is composed from three independent
clause families — how the givens are stated, which framing applies, how the ask
is put — and a *structure* is the triple. A relation with five given families, six
ask families and three framings has ninety structures, each independently
holdout-able.

The resolver rule is deliberately permissive, because a corpus narrower than the
sentences students write is the failure this system exists to fix. A structure is
refused only when nothing in the text settles which slot is which: two slots of
the same relation with the same dimension, in a given block that does not name
roles. Everything else is emitted, including nearly-decidable rows the abstain
gate can refuse at inference time.
"""

from collections import namedtuple

from . import lexicon, numbers
from .spec import SPECIES

Structure = namedtuple("Structure", "given ask framing names_roles id ask_first")
Row = namedtuple("Row", "text structure")


def _role(slot, register, variant):
    words = lexicon.role_words(slot, register)
    return words[variant % len(words)]


# Singular nouns that end in "s", so agreement does not turn "the amount of gas"
# into a plural. Only words whose last token is one of these are exempt.
SINGULAR_S = {"gas", "mass", "stress", "analysis", "glass", "series", "species",
              "physics", "celsius", "kelvin", "bus", "lens", "axis"}


def _is(role):
    """Crude number agreement, so "the electron domains is 3" never ships."""
    last = role.split()[-1].lower() if role.split() else role.lower()
    if last in SINGULAR_S:
        return "is"
    return "are" if last.endswith("s") and not last.endswith("ss") else "is"


def _value(slot, raw, register):
    text = numbers.canon(raw)
    if text is None:
        return None
    return numbers.spell(register, text, slot.unit)


# Each given family renders the whole block of givens. `names_roles` is the only
# claim that matters downstream: whether a reader is told which slot is which.
def _given_each(spec, parts, register, variant):
    clauses = []
    for index, (param, slot) in enumerate(spec.slots.items()):
        role = _role(slot, register, variant + index)
        clauses.append(f"The {role} {_is(role)} {parts[param]}.")
    return " ".join(clauses)


def _given_record(spec, parts, register, variant):
    pairs = [f"{_role(slot, register, variant + index)} {parts[param]}"
             for index, (param, slot) in enumerate(spec.slots.items())]
    return "The record lists " + "; ".join(pairs) + "."


def _given_equals(spec, parts, register, variant):
    clauses = [f"{_role(slot, register, variant + index).capitalize()} = {parts[param]}"
               for index, (param, slot) in enumerate(spec.slots.items())]
    return ". ".join(clauses) + "."


def _given_given(spec, parts, register, variant):
    pairs = [f"{_role(slot, register, variant + index)} of {parts[param]}"
             for index, (param, slot) in enumerate(spec.slots.items())]
    return "Given " + ", ".join(pairs) + ", work the rest out."


def _given_applied(spec, parts, register, variant):
    """The quantities as a scenario reports them, not as labelled slots.

    This is the applied-context register the sourcing report found we lacked: a
    device or a bench narrating its own readings. A slot whose dimension has no
    narration fragment keeps its labelled clause, so a scene never comes out half
    written.
    """
    clauses = []
    for index, (param, slot) in enumerate(spec.slots.items()):
        scene = lexicon.scene_phrase(slot, parts[param], variant + index)
        if scene is None:
            role = _role(slot, register, variant + index)
            clauses.append(f"The {role} {_is(role)} {parts[param]}.")
        else:
            clauses.append(f"In the setup, {scene}.")
    return " ".join(clauses)


def _given_equipment(spec, parts, register, variant):
    """The lab-sheet shape: an instrument, second person, a reading per slot.

    The register study measured apparatus in 31% of one source's items and second
    person in half of them, against 4% and 19% in the exam bank. The instrument is
    a context noun, not a rebuilt apparatus vocabulary.
    """
    clauses = []
    for index, (param, slot) in enumerate(spec.slots.items()):
        noun = lexicon.context_noun(slot, variant + index)
        role = _role(slot, register, variant + index)
        if noun is None:
            clauses.append(f"You record the {role} as {parts[param]}.")
        else:
            clauses.append(f"You load the {noun} and read the {role} as {parts[param]}.")
    return " ".join(clauses)


def _given_condition_first(spec, parts, register, variant):
    """A condition stated before the system, as the chemistry corpus does.

    The slot whose dimension is a condition (temperature or pressure) leads, so a
    row opens on "At 300 K, ..." rather than burying the condition mid-sentence.
    """
    order = list(spec.slots.items())
    lead = [item for item in order if item[1].dim in ("temperature", "pressure")]
    rest = [item for item in order if item not in lead]
    if not lead:
        return None
    first_param, first_slot = lead[0]
    clauses = [f"With the {_role(first_slot, register, variant)} at {parts[first_param]}"]
    for index, (param, slot) in enumerate(rest):
        role = _role(slot, register, variant + index)
        clauses.append(f"the {role} {_is(role)} {parts[param]}")
    return clauses[0] + ", " + ", ".join(clauses[1:]) + "."


def _given_declared(spec, parts, register, variant):
    """The chemistry corpus's declared list: a system, then values separated by semicolons.

    18% of the chemistry source's items hand the quantities over this way, against
    1% of the physics source and 1% of the exam bank, so it is a chemistry shape
    rather than a general one.
    """
    pairs = [f"{_role(slot, register, variant + index)} = {parts[param]}"
             for index, (param, slot) in enumerate(spec.slots.items())]
    return "For the system: " + "; ".join(pairs) + "."


def _short_role(slot):
    """A role without the equilibrium preamble, for narration mid-sentence."""
    return slot.role.replace("equilibrium concentration of ", "")


def _given_state(spec, parts, register, variant):
    """The equilibrium-from-a-state frame: a system arrives somewhere, then asks.

    Only relations that declare equilibrium get it; a rate or a titration question
    told it "settles at equilibrium" would be describing a different experiment.
    """
    if spec.name not in EQUILIBRIUM:
        return None
    clauses = []
    for index, (param, slot) in enumerate(spec.slots.items()):
        clauses.append(f"the {_short_role(slot)} at {parts[param]}")
    if len(clauses) == 1:
        return "The system settles at equilibrium, with " + clauses[0] + "."
    return ("The system settles at equilibrium, with "
            + ", ".join(clauses[:-1]) + " and " + clauses[-1] + ".")


def _given_species(spec, parts, register, variant):
    """Formula-led shape: the substance names the row, then the declared values.

    The species is a constant from the spec, used nowhere but here -- it is the
    subject of the sentence, not a quantity, and the relations it is offered to
    are exactly the ones whose arithmetic it cannot change.
    """
    species = SPECIES.get(spec.name)
    if species is None:
        return None
    pairs = [f"{_role(slot, register, variant + index)} = {parts[param]}"
             for index, (param, slot) in enumerate(spec.slots.items())]
    return f"A sample of {species}: " + "; ".join(pairs) + "."


def _given_list(spec, parts, register, variant):
    values = ", ".join(parts[param] for param in spec.slots)
    verb = "are" if len(parts) > 1 else "is"
    noun = "measurements" if len(parts) > 1 else "measurement"
    return f"The {noun} {verb} {values}."


def _always(spec):
    return True


def _has_condition(spec):
    return any(slot.dim in ("temperature", "pressure") for slot in spec.slots.values())


def _has_context(spec):
    return any(lexicon.context_noun(slot, 0) for slot in spec.slots.values())


# The last field says whether the family can render this relation at all. A
# family offered where it cannot apply would produce "None" and call it a
# sentence, so applicability is declared rather than discovered at render time.
GIVEN = (
    ("each", _given_each, True, _always),
    ("record", _given_record, True, _always),
    ("equals", _given_equals, True, _always),
    ("given", _given_given, True, _always),
    ("list", _given_list, False, _always),
    ("applied", _given_applied, True, _always),
    ("equipment", _given_equipment, True, _has_context),
    ("condition", _given_condition_first, True, _has_condition),
    ("declared", _given_declared, True, _always),
    ("state", _given_state, True, lambda spec: spec.name in EQUILIBRIUM),
    ("species", _given_species, True, lambda spec: spec.name in SPECIES),
)

# A constant phrase that decorates a row's context without doing arithmetic. It
# is refused where the phrase would restate the answer: a percent-yield row told
# "for this yield" is the double-decoration bug, so each modifier declares the
# words that disqualify it and the answer words are checked against them.
MODIFIERS = {
    "partial_pressure": (" The gases do not react with each other.", ()),
    "molarity": (" The solute is transferred completely.", ()),
    "dilution": (" The flask is made up to the mark afterwards.", ()),
    "dilution_v1": (" The stock is measured out with a pipette.", ()),
    "avg_rate": (" The concentration is tracked as the run proceeds.", ()),
    "rate_factor": (" The two runs differ in one condition at a time.", ()),
    "ph": (" The solution is held at 25 C.", ()),
    "poh": (" The solution is held at 25 C.", ()),
    "entropy_change": (" No heat is lost to the surroundings.", ()),
    "wavelength_from_frequency": (" The wave travels in a vacuum.", ()),
}

# Relations whose rows can honestly be described as arriving at an equilibrium
# state. Everything else keeps the frames that describe a measurement.
# Shapes from the register study this generator does not carry, with the reason.
# Recorded rather than left implicit, in the same spirit as the deferred maths
# pairs: an absence someone can read beats an absence they have to notice.
DEFERRED_SHAPES = {
    "formula-led (a species formula, then conditions)":
        "the relation contract has no species argument, so a row could state a "
        "formula it cannot bind to a quantity; 56% of the chemistry source is "
        "written this way, and it needs a contract change, not a clause",
    "rate, yield or percent as a modifier on a species":
        "same cause: the modifier attaches to a species the call does not take",
    "the verbal ask (compare, justify, predict)":
        "33% of the exam bank asks for an explanation; this architecture refuses "
        "it honestly rather than pretending to answer, so it is an abstention "
        "example and not a training row",
}

EQUILIBRIUM = {
    "kc_from_conc", "ksp_from_solubility", "ph", "poh", "ph_from_poh",
    "h_from_ph", "buffer_ph", "percent_dissociation",
}

# The third field says whether the ask needs a unit to write; the fourth whether
# it needs one to avoid writing. A pH has no unit, so "give it with its unit"
# is never offered for it.
ASK = (
    ("what", "What is the {answer}?", False, False),
    ("find", "Find the {answer}.", False, False),
    ("give", "Give the {answer}, with its unit.", True, False),
    ("report", "Report the {answer} to three significant figures.", False, True),
    ("howmuch", "How much is the {answer}?", False, False),
    ("express", "Express the {answer} in {unit}.", True, False),
    ("worked", "The next line of the solution asks for the {answer}.", False, False),
    ("applied", "What {answer} does the setup give?", False, False),
    ("estimate", "Estimate the {answer} to one significant figure.", False, False),
)

# A framing is only offered when the relation has the dimensions it talks about,
# so a molality question is never told to treat the gases as ideal.
FRAMING = (
    ("", (), lambda dims: True),

    ("volumes", ("volume",), lambda dims: dims.count("volume") >= 2),
    ("constant_t", ("temperature",), lambda dims: "temperature" in dims),
    ("ideal_gas", ("pressure", "volume"),
     lambda dims: "pressure" in dims and "volume" in dims),
    ("dissociation", ("concentration",), lambda dims: "concentration" in dims),
)

# Which relations declare the idealisation clause. It removes a quantity from
# consideration rather than binding arithmetic, so it only belongs where the
# quantity it removes is genuinely ignorable for that relation.
IDEALISED = {
    "specific_heat": ("containers",),
    "calorimetry_c": ("containers",),
    "calorimetry_mass": ("containers",),
    "latent_heat": ("containers",),
    "conduction_rate": ("edges",),
    "radiation_power": ("surroundings",),
    "gas_heat_cp": ("containers",),
    "entropy_change": ("reversibility",),
    "wave_speed": ("medium",),
    "double_slit_fringe": ("slits",),
    "doppler_freq": ("medium",),
    "mixture_final_temperature": ("containers",),
    "heat_reaction": ("containers",),
}

# An idealisation removes one quantity from consideration, so the clause has to
# name the quantity it removes; a single sentence for every relation would tell a
# Doppler question to neglect the container's heat capacity.
FRAMING_TEXT = {
    "": "",
    "idealisation:containers": " Assume the container takes up none of the heat.",
    "idealisation:edges": " Assume no heat escapes from the edges.",
    "idealisation:surroundings": " Treat the surroundings as much colder than the surface.",
    "idealisation:reversibility": " Treat the process as reversible.",
    "idealisation:medium": " Treat the medium as uniform and still.",
    "idealisation:slits": " Treat the slits as vanishingly narrow.",
    "volumes": " Assume the volumes are additive.",
    "constant_t": " The temperature does not change.",
    "ideal_gas": " Treat the gas as ideal.",
    "dissociation": " Assume the solute dissociates completely.",
}


# A framing is a claim about the situation, so it is declared per relation rather
# than inferred from the dimensions: "assume complete dissociation" is true of a
# molarity question and false of a rate question that happens to mention a
# concentration.
RELATION_FRAMINGS = {
    "gas_p": ("ideal_gas",),
    "combined_gas": ("ideal_gas",),
    "boyles": ("ideal_gas",),
    "charles": ("ideal_gas",),
    "partial_pressure": ("ideal_gas",),
    "gas_mixture_pressure": ("ideal_gas", "volumes"),
    "molarity": ("volumes",),
    "dilution": ("volumes",),
    "dilution_v1": ("volumes",),
    "mixing_concentration": ("volumes",),
    "percent_dissociation": ("dissociation",),
    "ph": ("constant_t",),
    "poh": ("constant_t",),
    "ph_from_poh": ("constant_t",),
    "h_from_ph": ("constant_t",),
}


def modifier_for(spec):
    """The constant phrase this relation can carry, or None when it would restate it."""
    entry = MODIFIERS.get(spec.name)
    if entry is None:
        return None
    text, forbidden = entry
    answer = " ".join(spec.answer_words) + " " + spec.answer.role
    if any(word in answer for word in forbidden):
        return None
    return text


def framings_for(spec):
    declared = RELATION_FRAMINGS.get(spec.name, ())
    dims = [slot.dim for slot in spec.slots.values()]
    allowed = {name for name, _, applies in FRAMING if applies(dims)}
    chosen = tuple(name for name in declared if name in allowed)
    if modifier_for(spec):
        chosen += ("modifier",)
    if spec.name in IDEALISED:
        chosen += tuple(f"idealisation:{kind}" for kind in IDEALISED[spec.name])
    return ("",) + chosen


def structures(spec):
    """Every structure this relation admits, refused ones included in the count."""
    names = framings_for(spec)
    for given_name, _, names_roles, applies in GIVEN:
        if not applies(spec):
            continue
        for ask_name, _, needs_unit, forbids_unit in ASK:
            if needs_unit and not spec.answer.unit:
                continue
            if forbids_unit and not spec.answer.unit:
                continue
            for framing in names:
                for ask_first in (False, True):
                    # Where the ask sits is a register fact, not a style choice:
                    # 39% of exam items ask in the last sentence, a third of the
                    # lab register asks mid-sentence, and a generator trained on
                    # one position fails on the other.
                    position = "ask-first" if ask_first else "ask-last"
                    yield Structure(
                        given=given_name,
                        ask=ask_name,
                        framing=framing,
                        names_roles=names_roles,
                        id=f"{given_name}|{ask_name}|{framing}|{position}",
                        ask_first=ask_first,
                    )


def refusal(spec, structure):
    """Why a reader could not resolve this row, or None when they could."""
    if structure.names_roles:
        return None
    for pair in spec.same_dim:
        units = {spec.slots[param].unit for param in pair}
        if len(units) == 1:
            return ("two slots share a dimension and the same unit: "
                    + ", ".join(pair))
    return None


def render(spec, args, structure, register, variant):
    """The sentence for one structure, or None when a value cannot be written plainly."""
    parts = {}
    for index, (param, slot) in enumerate(spec.slots.items()):
        text = _value(slot, args[index], register)
        if text is None:
            return None
        parts[param] = text
    given = {name: fn for name, fn, _, _ in GIVEN}[structure.given]
    body = given(spec, parts, register, variant)
    ask_words = lexicon.answer_words(spec, register)
    answer = ask_words[variant % len(ask_words)]
    ask_text = {name: text for name, text, _, _ in ASK}[structure.ask]
    ask = ask_text.format(answer=answer, unit=spec.answer.unit or "the given unit")
    if structure.ask_first:
        text = f"{ask} {body}{FRAMING_TEXT.get(structure.framing, '')}"
    else:
        text = f"{body} {ask}{FRAMING_TEXT.get(structure.framing, '')}"
    if structure.framing == "modifier":
        text += modifier_for(spec) or ""
    return Row(text=" ".join(text.split()), structure=structure)
