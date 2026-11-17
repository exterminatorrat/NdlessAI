"""Classify every item in the ingested AP Chemistry bank by what can answer it.

    uv run --with pypdf --with pypdfium2 --with pillow --with numpy \
        python apchem_ingest.py --corpus "..." --out corpora/apchem
    python apchem_audit.py --corpus corpora/apchem
    python apchem_audit.py --corpus corpora/apchem --sample 30 --seed 7
    python apchem_audit.py --corpus corpora/apchem --fixture

--out writes a verdict per row with a 160-character \`stem_preview\` for reading
and never for answering. --fixture writes the full text an evaluator needs, split
by the same buckets, as \`eval.jsonl\` and \`train.jsonl\` inside the corpus.

The question this answers is not "how many questions are there" but "how many can
this program answer as it is built". NdlessAI emits one fixed-arity call and
prints an exact number, or refuses; it does not explain, draw, rank or read a
graph. So every item lands in exactly one bucket, and the buckets are ordered so
the strict verdict wins:

  relation / relation_partial   an existing relation's shape, and whether the
                                quantities it needs are actually present
  typed                         the expression engine, no model
  new_relation / numeric_unmapped   one numeric answer, one step, no relation
  unanswerable_*                explain, figure, multi-step, design, setup,
                                open-ended — the architecture cannot print these
                                however many relations exist
  broken                        extraction did not survive; not judged on content

Classification is a rule table, so it can be wrong, and the way to keep a rule
table honest is to publish the rules and check a sample by hand. It is also only
a rule table: an item moving from an unnamed bucket into `relation` because a
pattern learned a new phrasing is a **coverage** change, never a capability
change, and the two are reported apart. A rule loosened this way names the items
it newly claims in the thread report, because a rule that catches more items
without saying which is indistinguishable from a rule that lies.

Likewise `quantity_lost` is a fact about the source — the question asked for a
quantity and the page drew its name as a picture — and it is kept out of the
catalogue-gap count so that a limit of the PDF is never booked as a limit of the
program. `--rules` prints
the table, `--sample` prints N items with their verdict for that check.

Nothing here writes the copyright text anywhere but the gitignored corpus: the
report prints counts, relation names and at most a truncated stem.
"""

import argparse
import collections
import hashlib
import json
import os
import re

UNIT_TITLES = {
    1: "Atomic Structure and Properties",
    2: "Molecular and Ionic Compound Structure and Properties",
    3: "Intermolecular Forces and Properties",
    4: "Chemical Reactions",
    5: "Kinetics",
    6: "Thermodynamics",
    7: "Equilibrium",
    8: "Acids and Bases",
    9: "Applications of Thermodynamics",
}

CATEGORIES = (
    "relation",
    "relation_partial",
    "typed",
    "new_relation",
    "numeric_unmapped",
    "unanswerable_explain",
    "unanswerable_figure",
    "unanswerable_multistep",
    "unanswerable_design",
    "unanswerable_setup",
    "unanswerable_open",
    "quantity_lost",
    "options_lost",
    "broken",
)

ANSWERABLE = {"relation", "typed"}

NUMBER = re.compile(r"(?<![\w.])(\d+(?:\.\d+)?)(?:\s*[eE]\s*[-+]?\d+)?(?![\w])")

# A quantity with a unit, or a bare number — what an option looks like when the
# answer is a number rather than a sentence.
NUMERIC_OPTION = re.compile(
    r"^[\s(<]*(?:[-+]?\d+(?:[.,]\d+)?(?:\s*[×xX]\s*10\s*[-+]?\d+)?|"
    r"infinity|zero|none)\s*"
    r"(?:%|atm|bar|kpa|pa|mmhg|torr|k|°c|°f|c|g|mg|kg|mol|mmol|m|cm|mm|nm|pm|"
    r"ml|l|l/mol|mol/l|m/s|kj|j|kj/mol|j/mol|j/(mol·k)|g/mol|amu|ev|v|mv|a|"
    r"kj/kg|degrees?|moles?|grams?|liters?|seconds?|minutes?|hours?|days?)?[\s).,]*$",
    re.I,
)

EXPLAIN = re.compile(
    r"\b(explain|justify|discuss|describe|give a reason|account for|"
    r"support your answer|in terms of|provide reasoning|rationalize)\b", re.I)
FIGURE = re.compile(
    r"\b(shown below|shown above|shown in the|graph|plot|diagram|figure|"
    r"table above|table below|in the table|following data|data table|"
    r"bar chart|particle representation|represented below|calibration line)\b",
    re.I)
DESIGN = re.compile(
    r"\b(design an experiment|experimental procedure|best procedure|"
    r"which procedure|should the student do|to test the|"
    r"to determine whether|experimental design|which of the following (should|"
    r"would best|is the best))\b", re.I)
# "Calculate the value of ." and "the mass of  that contains  of  ." — the
# question asks for a quantity and the page did not carry its name, because the
# species or symbol is a drawn image. That is a fact about the source, and it is
# reported apart from a gap in the catalogue.
QUANTITY_HOLE = re.compile(
    r"\b(calculate|compute|determine|find|what is)\b[^.?]{0,40}\b(value|mass|concentration|"
    r"pressure|volume|energy|amount|number|ratio|constant)\b[^.?]{0,20}\bof\s*(that|is|in|at|for|"
    r"when|\?|\.|$)|\b(of|that contains)\s+(of|\.)\s")
SETUP = re.compile(
    r"\b(show (the )?setup|write the (expression|equation|setup)|"
    r"set up the calculation|write a balanced|complete the (equation|table))\b",
    re.I)
MULTI = re.compile(
    r"\b(using your answer|based on your answer|from part|in part|"
    r"and then|after that|from step \d|using the (value|result|answer) (from|you))\b", re.I)
DRAW = re.compile(r"\b(draw|sketch|label|plot the|graph the)\b", re.I)
SCALAR_PROMPT = re.compile(
    r"\b(calculate|determine|what is|what will|how many|how much|find|compute|"
    r"what mass|what volume|what pressure|what concentration|what percentage|"
    r"closest to)\b", re.I)

# An existing relation fires on the wording that asks for its quantity. The
# count is the number of quantities that must appear in the question, taken from
# the relation's own parameter list; a match with fewer falls to
# `relation_partial`, which is the extraction-loss verdict rather than a gap.
RELATION_RULES = (
    ("molarity", r"\bmolarity\b|\bwhat is the (molar )?concentration\b|\bmol/l\b"
                 r"|\bconcentration of\b.*\b(resulting|solution)\b", 2),
    ("dilution", r"\bdilut(e|ed|ion)\b|\bneeded to prepare\b", 3),
    ("moles", r"\bhow many (moles|mol)\b|\bnumber of moles\b|\bmoles of\b", 2),
    ("mass_percent", r"\bmass percent\b|\bpercent by mass\b|\bpercent of \w+ by mass\b", 2),
    ("molality", r"\bmolality\b|\bmol/kg\b", 2),
    ("mole_fraction", r"\bmole fraction\b", 2),
    ("percent_yield", r"\bpercent(age)? yield\b|\byield of\b.*%", 2),
    ("buffer_ph", r"\bbuffer\b.*\bpH\b|\bpKa\b", 3),
    ("poh", r"\bpOH\b", 1),
    ("ph", r"\bpH\b", 1),
    ("kc_from_conc", r"\bKc\b|\bKp\b|\bequilibrium constant\b", 3),
    ("gibbs", r"\bΔG\b|\bGibbs\b", 3),
    ("specific_heat", r"\bspecific heat\b|\bJ/\(g", 3),
    ("calorimetry_c", r"\bcalorimeter\b|\bheat (released|absorbed)\b", 3),
    ("latent_heat", r"\bheat of (fusion|vaporization|sublimation)\b|\blatent\b"
                   r"|\b(energy|heat)\b[^.?]{0,60}\b(melt|freeze|vaporiz\w*|condens\w*)\b", 2),
    ("heat_reaction", r"\bΔH\b.*\bmol", 2),
    ("limiting_moles", r"\blimiting (reactant|reagent)\b", 4),
    ("stoich_mass", r"\b(grams|mass) of\b.*\b(produced|formed|to produce|necessary)\b", 4),
    ("stoichiometry", r"\bstoichiometr", 2),
    ("combined_gas", r"\bideal gas\b|\bPV ?= ?nRT\b"
                     r"|\brigid (container|flask|vessel)\b.*\bpressure\b", 4),
    ("boyles", r"\bconstant temperature\b|\bBoyle\b", 3),
    ("charles", r"\bconstant pressure\b|\bCharles\b|\bmovable piston\b", 3),
    ("gas_p", r"\bpressure\b.*\bvolume\b.*\btemperature\b", 3),
    ("avg_rate", r"\baverage rate\b|\brate of (reaction|disappearance|formation)\b", 2),
    ("density", r"\bdensity\b", 2),
    ("half_life", r"\bhalf[- ]life\b", 3),
    ("percent_error", r"\bpercent error\b", 2),
    ("sig_figs", r"\bsignificant figures?\b", 1),
    ("scientific_notation", r"\bscientific notation\b", 2),
    ("photon_energy_ev", r"\benergy of (a|the) photon\b|\bphoton\b.*\benergy\b", 1),
    ("de_broglie_electron", r"\bde Broglie\b", 1),
    ("bohr_transition_nm", r"\b(n ?= ?\d)\b.*\b(n ?= ?\d)\b|\btransition\b.*\bn ?= ?\d", 2),
    ("celsius_to_fahrenheit", r"\bdegrees? (Fahrenheit|Celsius)\b", 1),
    ("km_to_m", r"\bkilometers?\b.*\bmeters?\b", 1),
    ("liters_to_ml", r"\bliters?\b.*\bmilliliters?\b", 1),
    ("percent_of", r"\bwhat (mass|volume|amount)\b.*\bof the\b", 2),
    ("ratio_share", r"\bratio\b", 2),
)

# The quantities AP Chemistry asks for that the catalogue has no relation for.
# Each entry is the relation this bank says is missing and the arguments it would
# take, which is what makes the gap list actionable rather than a complaint.
GAP_RULES = (
    ("electron_configuration", r"\belectron configuration\b", "(n_electrons) -> notation"),
    ("unpaired_electrons", r"\bunpaired electrons?\b", "(config) -> count"),
    ("ionization_energy", r"\bionization energy\b", "(element, n) -> kJ/mol"),
    ("effective_nuclear_charge", r"\beffective nuclear charge\b", "(Z, shielding) -> Z_eff"),
    ("lattice_energy", r"\blattice energy\b", "(q1, q2, r) -> kJ/mol"),
    ("formal_charge", r"\bformal charge\b", "(valence, nonbonding, bonds) -> charge"),
    ("oxidation_number", r"\boxidation (state|number)\b", "(element, compound) -> state"),
    ("empirical_formula", r"\bempirical formula\b", "(percent_composition) -> formula"),
    ("percent_composition", r"\bpercent composition\b", "(formula, element) -> percent"),
    ("bond_order", r"\bbond order\b", "(bonding, antibonding) -> order"),
    ("hybridization", r"\bhybridization\b", "(domains) -> sp^n"),
    ("partial_pressure", r"\bpartial pressure\b|\bmole fraction\b.*\bpressure\b",
     "(n_i, n_total, P_total) -> atm"),
    ("gas_mixture_pressure", r"\bare combined in\b|\bare pumped into\b",
     "(P_i, V_i, V_f) -> atm"),
    ("mass_from_molarity", r"\bmass of\b.*\brequired to prepare\b|\brequired to prepare\b",
     "(c, V, molar_mass) -> g"),
    ("mixture_final_temperature", r"\bis mixed with\b.*\btemperature\b",
     "(m1, T1, m2, T2) -> K"),
    ("kc_from_pressures", r"\b(Kc|Kp|equilibrium constant)\b.*\bpressures?\b"
                          r"|\bpressures?\b.*\b(Kc|Kp)\b",
     "(P0, P_eq, coefficients) -> Kc"),
    ("stoich_volume_gas", r"\bvolume of\b.*\brequired to react\b",
     "(V_given, ratio) -> L"),
    ("percent_composition_reverse", r"\bpercent of \w+ in the sample\b|\breported as being\b",
     "(percent_element, formula) -> percent"),
    ("vsepr_bond_angle", r"\bbond angle\b", "(geometry) -> degrees"),
    ("ph_of_water_kw", r"\bof pure water\b|\bKw\b", "(Kw, T) -> pH"),
    ("atoms_from_mass", r"\bhow many \w* ?(atoms|molecules|ions)\b"
                        r"|\bnumber of (atoms|molecules)\b",
     "(mass, molar_mass) -> count"),
    ("balance_coefficients", r"\bis balanced\b|\ball coefficients\b",
     "(equation) -> coefficients"),
    ("hydrate_water", r"\bhydrate\b|\bwater of (hydration|crystallization)\b",
     "(mass_hydrate, mass_anhydrous) -> formula"),
    ("mixing_concentration", r"\bis added to\b.*\b(milliliters?|mL|molar)\b",
     "(c1, v1, c2, v2) -> M"),
    ("electrons_transferred", r"\belectrons? (are )?transferred\b|\bhow many electrons\b",
     "(half_reactions) -> count"),
    ("reaction_quotient", r"\breaction quotient\b", "(conc, Kc) -> Q"),
    ("electron_domain_geometry", r"\b(VSEPR|electron domain|molecular geometry)\b", "(domains, lone) -> geometry"),
    ("average_atomic_mass_isotopes", r"\baverage atomic mass\b|\bisotopic (mass|abundance)\b", "(mass_i, abundance_i) -> amu"),
    ("integrated_rate_first_order", r"\bfirst[- ]order\b", "(k, t, [A]0) -> [A]t"),
    ("integrated_rate_second_order", r"\bsecond[- ]order\b", "(k, t, [A]0) -> [A]t"),
    ("rate_law_from_data", r"\brate law\b|\borders? of reaction\b", "(rate_series) -> law"),
    ("arrhenius_activation_energy", r"\bactivation energy\b|\bArrhenius\b", "(k1, k2, T1, T2) -> Ea"),
    ("ksp_from_solubility", r"\bKsp\b|\bsolubility product\b", "(s, formula) -> Ksp"),
    ("titration_volume", r"\btitrat(e|ed|ion)\b", "(c_a, v_a, c_b, ratio) -> mL"),
    ("weak_acid_ka", r"\bKa\b|\bacid dissociation\b", "(c0, Ka) -> pH"),
    ("percent_dissociation", r"\bpercent (dissociation|ionization)\b", "(c0, Ka) -> percent"),
    ("cell_potential", r"\b(cell potential|E°?cell|standard reduction potential)\b", "(E_red, E_ox) -> V"),
    ("nernst", r"\bNernst\b", "(E0, n, Q) -> V"),
    ("faraday_deposit", r"\belectroly\w+\b|\bFaraday\b|\bcurrent of\b", "(current, time, n) -> g"),
    ("entropy_change", r"\bΔS\b|\bentropy\b", "(states) -> J/K"),
    ("graham_effusion", r"\b(effusion|Graham)\b", "(m1, m2) -> ratio"),
    ("colligative_freezing", r"\bfreezing point\b|\bboiling point elevation\b|\bosmotic pressure\b", "(molality, Kf) -> °C"),
    ("beer_lambert", r"\babsorbance\b|\bBeer", "(A, epsilon, l) -> c"),
    ("heating_curve_energy", r"\bheating curve\b", "(m, c, dT, L) -> J"),
    ("bond_energy_enthalpy", r"\bbond energ(y|ies)\b", "(broken, formed) -> kJ/mol"),
    ("van_der_waals", r"\bvan der Waals\b", "(n, V, T, a, b) -> P"),
    ("unit_cell_density", r"\bunit cell\b", "(a, Z, M) -> g/cm3"),
    ("quantum_numbers", r"\bquantum numbers?\b", "(n, l, m) -> allowed"),
    ("wavelength_frequency", r"\b(wavelength|frequency)\b", "(nu, lambda) -> m"),
    ("coulombic_potential_energy", r"\bCoulomb'?s law\b|\bpotential energy of (the )?ions?\b", "(q1, q2, r) -> J"),
    ("collision_frequency_orientation", r"\b(collision|effective collision)\b", "(theory) -> rate"),
)


# Where a species should be and the page has none: "the mass percent of in
# eggshells", "the ion is green", "between  and ". This is the shape the drawn
# formulas leave behind in the text layer, and counting it measures the loss
# without needing the recogniser that would repair it.
LOST_SPECIES = re.compile(
    r"\b(of|the|and|between|in|from|for|with|to|as|than)\s+(in|is|are|were|was|has|have|at|"
    r"and|with|to|because|that|which|when|it|its|shown|below|above)\b", re.I)


def numbers(text):
    return [float(n) for n in NUMBER.findall(text or "")]


def option_is_numeric(text):
    if not text:
        return False
    if NUMERIC_OPTION.match(text):
        return True
    # `2.5 g of H2` style options, and equations the option body starts with a
    # number but then explains itself, are the case this deliberately misses.
    return bool(re.match(r"^[\s(<]*[-+]?\d+(?:[.,]\d+)?\s*$", text))


def numeric_options(options):
    """Share of the options that state a quantity, over the ones that survived.

    Empty options are measured separately as lost text rather than counted as
    non-numeric, because a missing equation is not evidence that the answer is a
    sentence and it would quietly inflate the qualitative count.
    """

    values = [text for _, text in sorted((options or {}).items()) if text and text.strip()]
    if not values:
        return 0.0
    return sum(1 for value in values if option_is_numeric(value)) / len(values)


def body(item):
    """The whole question as text, for keyword tests only."""

    parts = [item.get("stem", ""), item.get("stimulus", "")]
    for part in item.get("parts", []):
        parts.append(part.get("text", ""))
        for sub in part.get("subs", []):
            parts.append(sub.get("text", ""))
    return " ".join(parts)


def first_relation(text):
    for name, pattern, needed in RELATION_RULES:
        if re.search(pattern, text, re.I):
            return name, needed
    return None, 0


def first_gap(text):
    for name, pattern, shape in GAP_RULES:
        if re.search(pattern, text, re.I):
            return name, shape
    return None, None


def classify_mcq(item):
    options = item.get("options") or {}
    filled = [value for value in options.values() if value and value.strip()]
    text = body(item)
    if len(filled) < 3 or len(text.strip()) < 40:
        return "broken", None, "fewer than three options survived"

    # The cue is read from the item's own stem. A stimulus that mentions a table
    # is shared by two to four questions and only some of them read it, so
    # matching the whole set flagged its simpler siblings as unanswerable.
    if FIGURE.search(item.get("stem", "")):
        return "unanswerable_figure", None, "needs a graph, table or diagram read"
    if EXPLAIN.search(text):
        return "unanswerable_explain", None, "asks for reasoning, not a quantity"
    if DESIGN.search(text):
        return "unanswerable_design", None, "asks about method or procedure"

    share = numeric_options(options)
    if share < 0.5:
        if len(filled) < len(options):
            return "options_lost", None, f"{len(filled)} of {len(options)} options survived"
        return "unanswerable_open", None, f"{share:.0%} of options numeric"

    relation, needed = first_relation(text)
    quant = item.get("stem", "") + " " + item.get("stimulus", "")
    if relation:
        if len(numbers(quant)) >= needed:
            return "relation", relation, f"{len(numbers(quant))} quantities, needs {needed}"
        return "relation_partial", relation, f"{len(numbers(quant))} of {needed} quantities"

    gap, shape = first_gap(text)
    if gap:
        return "new_relation", gap, shape
    if MULTI.search(text):
        return "unanswerable_multistep", None, "refers to an earlier result"
    if QUANTITY_HOLE.search(text):
        return "quantity_lost", None, "the quantity asked for is a drawn image"
    return "numeric_unmapped", None, "numeric answer, quantity not identified"


def classify_frq_part(item, part):
    text = " ".join([item.get("stem", ""), part.get("text", "")] +
                    [sub.get("text", "") for sub in part.get("subs", [])])
    if len(text.strip()) < 30:
        return "broken", None, "part text lost"
    if DRAW.search(text):
        return "unanswerable_figure", None, "asks for a drawing"
    if FIGURE.search(text) and re.search(r"\b(the (table|graph)|shown below|data)\b", text, re.I):
        return "unanswerable_figure", None, "needs a graph, table or diagram read"
    if EXPLAIN.search(text):
        return "unanswerable_explain", None, "asks for reasoning, not a quantity"
    if DESIGN.search(text):
        return "unanswerable_design", None, "asks about method or procedure"
    if SETUP.search(text):
        return "unanswerable_setup", None, "answer is a setup, not a number"
    if not SCALAR_PROMPT.search(text):
        return "unanswerable_open", None, "no numeric answer asked for"

    relation, needed = first_relation(text)
    if relation:
        if len(numbers(text)) >= needed:
            return "relation", relation, f"{len(numbers(text))} quantities, needs {needed}"
        return "relation_partial", relation, f"{len(numbers(text))} of {needed} quantities"
    gap, shape = first_gap(text)
    if gap:
        return "new_relation", gap, shape
    if part.get("subs") or MULTI.search(text):
        return "unanswerable_multistep", None, "several steps to the answer"
    if QUANTITY_HOLE.search(text):
        return "quantity_lost", None, "the quantity asked for is a drawn image"
    return "numeric_unmapped", None, "numeric answer, quantity not identified"


def worst(categories):
    """A prompt is answerable only when every part of it is."""

    for category in CATEGORIES:
        if category in categories:
            return category
    return "broken"


def audit_mcq(records):
    rows = []
    for item in records:
        category, target, why = classify_mcq(item)
        rows.append({
            "id": item["id"], "kind": "mcq", "unit": item.get("unit"),
            "category": category, "target": target, "why": why,
            "stem_preview": item.get("stem", "")[:160],
        })
    return rows


def audit_frq(records):
    rows = []
    for item in records:
        verdicts = []
        for part in item.get("parts", []):
            category, target, why = classify_frq_part(item, part)
            verdicts.append({
                "id": part["id"], "kind": "frq-part", "set": item.get("set"),
                "category": category, "target": target, "why": why,
                "stem_preview": part.get("text", "")[:160],
                "guide": bool(part.get("guide")),
            })
        rows.extend(verdicts)
        rows.append({
            "id": item["id"], "kind": "frq", "set": item.get("set"),
            "category": worst([v["category"] for v in verdicts]),
            "target": None,
            "why": f"{len(verdicts)} parts",
            "stem_preview": item.get("stem", "")[:160],
        })
    return rows


def split_of(identifier):
    """A stable bucket from the id alone, so the split survives any re-ingest."""

    return int(hashlib.sha256(identifier.encode()).hexdigest()[:8], 16)


def write_fixture(directory, mcq, frq, held_ids):
    """Full-text eval and train rows. A 160-character preview is not answerable text."""

    prompt_of = {part["id"]: item["id"] for item in frq for part in item.get("parts", [])}

    def side(identifier):
        # Half a prompt in each half would put the same rubric on both sides.
        return "eval" if prompt_of.get(identifier, identifier) in held_ids else "train"

    verdicts = {row["id"]: row for row in audit_mcq(mcq) + audit_frq(frq)}
    rows = {"eval": [], "train": []}
    for item in mcq:
        verdict, stream = verdicts[item["id"]], side(item["id"])
        rows[stream].append({
            "id": item["id"], "kind": "mcq", "unit": item.get("unit"), "split": stream,
            "category": verdict["category"], "target": verdict["target"],
            "why": verdict["why"], "stem": item.get("stem", ""),
            "stimulus": item.get("stimulus", ""), "options": item.get("options") or {},
            "answer": item.get("answer"),
        })
    for item in frq:
        for part in item.get("parts", []):
            verdict, stream = verdicts[part["id"]], side(part["id"])
            rows[stream].append({
                "id": part["id"], "kind": "frq-part", "set": item.get("set"),
                "split": stream, "category": verdict["category"],
                "target": verdict["target"], "why": verdict["why"],
                "stem": item.get("stem", ""), "text": part.get("text", ""),
                "guide": bool(part.get("guide")),
            })
    for stream, stream_rows in rows.items():
        with open(os.path.join(directory, f"{stream}.jsonl"), "w") as handle:
            for row in stream_rows:
                handle.write(json.dumps(row, ensure_ascii=False) + "\n")
    return rows


def main():
    argument = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    argument.add_argument("--corpus", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "corpora", "apchem"))
    argument.add_argument("--out", default=None, help="write every verdict as JSONL")
    argument.add_argument("--sample", type=int, default=0,
                          help="print N classified items for a hand check")
    argument.add_argument("--seed", type=int, default=1)
    argument.add_argument("--rules", action="store_true", help="print the rule table")
    argument.add_argument("--fixture", action="store_true",
                          help="write full-text eval.jsonl and train.jsonl into the corpus")
    argument.add_argument("--eval-ids", default=None,
                          help="pin the eval side to a frozen id list instead of a fresh split")
    argument.add_argument("--eval-share", type=float, default=0.25,
                          help="share of each stratum held back as eval")
    args = argument.parse_args()

    if args.rules:
        print("relation triggers")
        for name, pattern, needed in RELATION_RULES:
            print(f"  {name:28s} needs {needed}  {pattern}")
        print("gap triggers")
        for name, pattern, shape in GAP_RULES:
            print(f"  {name:32s} {shape:34s} {pattern}")
        return 0

    mcq = [json.loads(line) for line in open(os.path.join(args.corpus, "mcq.jsonl"))]
    frq = [json.loads(line) for line in open(os.path.join(args.corpus, "frq.jsonl"))]
    mcq_rows = audit_mcq(mcq)
    frq_rows = audit_frq(frq)

    print("== extraction quality ==")
    complete = sum(1 for item in mcq if len(item.get("options") or {}) >= 4
                   and all(v.strip() for v in item["options"].values()))
    gap = sum(1 for item in mcq if LOST_SPECIES.search(item.get("stem", "") + " " +
                                                      item.get("stimulus", "")))
    keyed = sum(1 for item in mcq if item.get("answer"))
    print(f"mcq items              {len(mcq)}")
    print(f"  four filled options  {complete} ({complete/max(len(mcq),1):.1%})")
    print(f"  answer key           {keyed} ({keyed/max(len(mcq),1):.1%})")
    print(f"  a missing species    {gap} ({gap/max(len(mcq),1):.1%})")
    parts = sum(len(item.get("parts", [])) for item in frq)
    guided = sum(1 for item in frq for part in item.get("parts", []) if part.get("guide"))
    print(f"frq prompts            {len(frq)} in {parts} parts, {guided} parts with a guide")

    print("\n== mcq by unit ==")
    print("unit  items  " + " ".join(f"{i + 1:>5}" for i in range(len(CATEGORIES))))
    totals = collections.Counter()
    for unit in sorted(UNIT_TITLES):
        rows = [row for row in mcq_rows if row["unit"] == unit]
        if not rows:
            continue
        counter = collections.Counter(row["category"] for row in rows)
        totals.update(counter)
        print(f"{unit:>4}  {len(rows):>5}  " + " ".join(
            str(counter[name]).rjust(5) for name in CATEGORIES))
    print(f" all  {len(mcq_rows):>5}  " + " ".join(
        str(totals[name]).rjust(5) for name in CATEGORIES))
    for index, name in enumerate(CATEGORIES):
        print(f"  {index + 1:>2}. {name}")

    print("\n== frq by set ==")
    print("set   parts  relation  partial  new_rel  unmapped  explain  figure  setup  "
          "multistep  open  lost  broken")
    frq_totals = collections.Counter()
    for number in sorted({row["set"] for row in frq_rows}):
        rows = [row for row in frq_rows if row.get("set") == number and row["kind"] == "frq-part"]
        counter = collections.Counter(row["category"] for row in rows)
        frq_totals.update(counter)
        values = [counter[name] for name in ("relation", "relation_partial", "new_relation",
                                             "numeric_unmapped", "unanswerable_explain",
                                             "unanswerable_figure", "unanswerable_setup",
                                             "unanswerable_multistep", "unanswerable_open",
                                             "options_lost", "broken")]
        print(f"{number:>3}  {len(rows):>6}  " +
              "  ".join(str(value).rjust(8) for value in values))

    print("\n== existing relations the bank actually exercises ==")
    used = collections.Counter(row["target"] for row in mcq_rows + frq_rows
                               if row["category"] == "relation" and row["target"])
    for name, count in used.most_common():
        print(f"  {name:28s} {count}")

    print("\n== relations the bank needs and the catalogue lacks ==")
    gaps = collections.Counter(row["target"] for row in mcq_rows + frq_rows
                               if row["category"] == "new_relation" and row["target"])
    for name, count in gaps.most_common():
        shape = next((s for n, _, s in GAP_RULES if n == name), "")
        print(f"  {name:32s} {count:>4}  {shape}")

    print("\n== totals ==")
    # A free-response prompt is judged by its parts: the prompt row is the "all
    # of it or none of it" summary and counting it beside its parts would double
    # every free-response question in the bank.
    everything = mcq_rows + [row for row in frq_rows if row["kind"] == "frq-part"]
    counted = collections.Counter(row["category"] for row in everything)
    for name in CATEGORIES:
        print(f"  {name:24s} {counted[name]:>5}")
    answerable = sum(counted[name] for name in ANSWERABLE)
    print(f"  answerable today         {answerable:>5} of {len(everything)} "
          f"({answerable/max(len(everything),1):.1%})")

    print("\n== split ==")
    # Held back by prompt for free response: half a prompt in each half would put
    # the same rubric and the same stem on both sides of the line.
    split_rows = mcq_rows + [row for row in frq_rows if row["kind"] == "frq"]
    by_stratum = collections.defaultdict(list)
    for row in split_rows:
        by_stratum[(row["kind"], row.get("unit") or row.get("set"), row["category"])].append(row)
    evaluation, training = [], []
    for key, rows in sorted(by_stratum.items(), key=lambda kv: str(kv[0])):
        rows.sort(key=lambda row: split_of("apchem/" + row["id"]))
        cut = max(1, round(len(rows) * args.eval_share)) if len(rows) > 1 else 0
        evaluation.extend(rows[:cut])
        training.extend(rows[cut:])
    if args.eval_ids:
        # A frozen list outranks a fresh split: two runs over one bank have to
        # score the same rows, or their numbers are not comparable.
        with open(args.eval_ids) as handle:
            pinned = {line.strip() for line in handle if line.strip()}
        prompt_of = {part["id"]: item["id"] for item in frq for part in item.get("parts", [])}
        owners = {prompt_of.get(identifier, identifier) for identifier in pinned}
        evaluation = [row for row in split_rows if row["id"] in owners]
        training = [row for row in split_rows if row["id"] not in owners]
        absent = sorted(owners - {row["id"] for row in split_rows})
        print(f"  pinned to {len(pinned)} ids in {os.path.basename(args.eval_ids)}: "
              f"{len(evaluation)} eval, {len(training)} train")
        if absent:
            print(f"  absent from the corpus: {len(absent)} ({', '.join(absent[:6])})")
    else:
        print(f"  strata {len(by_stratum)}, eval {len(evaluation)}, train {len(training)}, "
              f"share {args.eval_share:.0%}")
    for kind in ("mcq", "frq"):
        held = [row for row in evaluation if row["kind"] == kind]
        total = [row for row in split_rows if row["kind"] == kind]
        print(f"  {kind}: {len(held)} of {len(total)} held back")
    held_answerable = sum(1 for row in evaluation if row["category"] in ANSWERABLE)
    print(f"  eval items answerable today: {held_answerable}")

    held_ids = {row["id"] for row in evaluation}
    if args.fixture:
        fixture = write_fixture(args.corpus, mcq, frq, held_ids)
        longest = max((len(row["stem"]) + len(row.get("text", ""))
                       for row in fixture["eval"]), default=0)
        print(f"  fixture eval {len(fixture['eval'])} rows, "
              f"train {len(fixture['train'])} rows")
        print(f"  longest eval stem {longest} characters")
    if args.out:
        with open(args.out, "w") as handle:
            for row in mcq_rows + frq_rows:
                owner = row["id"].rsplit("-", 1)[0] if row["kind"] == "frq-part" else row["id"]
                row["split"] = "eval" if owner in held_ids or row["id"] in held_ids else "train"
                handle.write(json.dumps(row, ensure_ascii=False) + "\n")
        print(f"\nwrote {args.out}")

    if args.sample:
        import random

        rng = random.Random(args.seed)
        print(f"\n== {args.sample} sampled verdicts ==")
        for row in rng.sample(everything, min(args.sample, len(everything))):
            print(f"  {row['id']:16s} {row['category']:22s} {str(row['target'] or ''):>24} "
                  f"| {row['why'][:38]} | {re.sub(chr(10), ' ', row['stem_preview'])[:70]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
