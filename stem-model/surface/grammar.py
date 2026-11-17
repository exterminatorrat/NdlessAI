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

Structure = namedtuple("Structure", "given ask framing names_roles id ask_first")
Row = namedtuple("Row", "text structure")


def _role(slot, register, variant):
    words = lexicon.role_words(slot, register)
    return words[variant % len(words)]


def _is(role):
    """Crude number agreement, so "the electron domains is 3" never ships."""
    return "are" if role.endswith("s") and not role.endswith("ss") else "is"


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
    clauses = [f"At a {_role(first_slot, register, variant)} of {parts[first_param]}"]
    for index, (param, slot) in enumerate(rest):
        role = _role(slot, register, variant + index)
        clauses.append(f"the {role} {_is(role)} {parts[param]}")
    return clauses[0] + ", " + ", ".join(clauses[1:]) + "."


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
)

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


def framings_for(spec):
    declared = RELATION_FRAMINGS.get(spec.name, ())
    dims = [slot.dim for slot in spec.slots.values()]
    allowed = {name for name, _, applies in FRAMING if applies(dims)}
    chosen = tuple(name for name in declared if name in allowed)
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
        text = f"{ask} {body}{FRAMING_TEXT[structure.framing]}"
    else:
        text = f"{body} {ask}{FRAMING_TEXT[structure.framing]}"
    return Row(text=" ".join(text.split()), structure=structure)
