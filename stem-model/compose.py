"""Which relations compose into a chain, derived from the tree's own metadata.

`runtime/plan.c` runs a chain of calls: a step's argument may be `<n>`, the
magnitude of step n's answer, and the runtime passes that number on without
looking at its unit. So the runtime cannot tell a chain that means something
from one that adds newtons to moles. Deciding which pairs compose is therefore a
*generation* problem, and this module is where the decision is made — from the
tree's declarations, not from a list of plausible pairs someone typed out.

Three sources, each with one job:

  - the reference solver in `relations/` answers with the string the app shows
    ("3.92 N", "x=-2, x=3"), so it states, for all 171 relations, what a step's
    answer is *and whether it is one number at all* — the same test `plan.c`'s
    magnitude_of applies before a chain may continue;
  - `runtime/unitstable.h` states the unit each argument is phrased with, for
    124 relations, generated from the phrasing bank by `export_units.py`. It is
    the only per-slot unit the runtime has today;
  - `surface/spec.py` declares a dimension for every slot and answer of 67
    relations, all 48 chemistry ones among them. It is used here as a second
    opinion: where both sources describe a slot, their dimensions must agree,
    and a disagreement is reported rather than resolved by picking one.

An edge `S -> R` means R has a slot that can take S's answer:

  1. S answers with a single number, measured by calling the reference and
     applying the runtime's own rule — an answer with '=' or ',' is several
     values and a plan refuses to chain it;
  2. S's answer unit and the slot's unit are equal after normalisation, or one
     is the other times a factor declared in `FACTORS`;
  3. where `spec.py` describes both, their dimensions are equal as well.

The factor table is the only hand-written list here, because a conversion is a
fact about units and not about a relation. Everything else is read.
"""

import argparse
import os
import random
import re
import sys
from collections import Counter, defaultdict

ROOT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ROOT)

from relations import RELATIONS  # noqa: E402
from surface.spec import SPECS  # noqa: E402

UNITSTABLE = os.path.join(ROOT, "runtime", "unitstable.h")

# Exact conversions between units the tree already writes. Kept short on
# purpose: an edge this table does not cover is missing rather than wrong, so
# the edge count below is a floor.
FACTORS = {
    ("kJ", "J"): 1000.0,
    ("J", "kJ"): 0.001,
    ("kJ/mol", "J/mol"): 1000.0,
    ("J/mol", "kJ/mol"): 0.001,
    ("L", "mL"): 1000.0,
    ("mL", "L"): 0.001,
    ("cm^3", "mL"): 1.0,
    ("mL", "cm^3"): 1.0,
    ("cm^3", "L"): 1000.0,
    ("kg", "g"): 1000.0,
    ("g", "kg"): 0.001,
    ("nm", "m"): 1e-9,
    ("kJ/(kg*K)", "J/(kg*K)"): 1000.0,
    ("J/(kg*K)", "kJ/(kg*K)"): 0.001,
}

# Spellings that mean the same unit. Two sources in this tree write the same
# quantity two ways — `spec.py` records what a student writes, `unitstable.h`
# what the phrasing bank states — and this is the only place the two are joined.
SAME = {
    "M": "mol/L",
    "molar": "mol/L",
    "molarity": "mol/L",
    "mol/L": "mol/L",
    "percent": "%",
    "%": "%",
    "degrees": "degrees",
    "m/s2": "m/s^2",
    "litre": "L",
    "liter": "L",
    "litres": "L",
    "liters": "L",
    "L": "L",
    "mL": "mL",
    "cm3": "cm^3",
    "K": "K",
}

# Dimensions that name the same physical quantity at different specificity. A
# wavelength *is* a length and an enthalpy change *is* an energy, so an edge
# between them is sound; the pair is written out rather than inferred because
# the direction matters — a length is not always a wavelength, and the slot is
# the one that says which it wants.
REFINES = {
    ("length", "wavelength"),
    ("enthalpy", "energy"),
}

MAX_CALLS = 4


def canonical(unit):
    unit = (unit or "").strip()
    return SAME.get(unit, unit)


def load_slot_units():
    """`unitstable.h` as {relation: [unit per argument]}."""
    units = {}
    pattern = re.compile(r'\{"([a-z0-9_]+)",\s*"([^"]*)"\}')
    with open(UNITSTABLE, encoding="utf-8") as handle:
        for line in handle:
            match = pattern.search(line)
            if not match:
                continue
            name, spec = match.group(1), match.group(2)
            units[name] = [canonical(part) if part != "-" else None
                           for part in spec.split()]
    return units


def answer_of(name):
    """The reference solver's own answer string for one sample of arguments."""
    relation = RELATIONS[name]
    for seed in range(8):
        try:
            args, _text = relation.gen(random.Random(seed))
            return relation.solve(*args)
        except Exception:
            continue
    return None


def chainable_and_unit(answer):
    """Whether the answer can feed a step, and the unit it carries.

    The rule is the runtime's: `plan.c`'s magnitude_of refuses an answer holding
    '=' or ',' (several values) and takes the leading numeric run, so everything
    after that run is the unit as the app prints it.
    """
    if answer is None or "=" in answer or "," in answer:
        return False, None
    match = re.match(r"[+-]?[0-9.]+(?:[eE][+-]?\d+)?\s*(.*)$", answer)
    if not match:
        return False, None
    return True, canonical(match.group(1))


def convert(source_unit, target_unit):
    """The factor from a source unit to a target unit, or None."""
    if source_unit == target_unit:
        return 1.0
    return FACTORS.get((source_unit, target_unit))


def build_graph():
    """The composition graph, and the disagreements found while building it."""
    table = load_slot_units()
    answers = {name: chainable_and_unit(answer_of(name)) for name in RELATIONS}
    graph = defaultdict(list)
    notes = []
    for source, (ok, unit) in answers.items():
        if not ok:
            continue
        source_dim = SPECS[source].answer.dim if source in SPECS else None
        for target, spec in RELATIONS.items():
            described = SPECS.get(target)
            listed = table.get(target) or []
            for index, param in enumerate(spec.params):
                slot_unit = listed[index] if index < len(listed) else None
                slot_dim = None
                if described is not None and param in described.slots:
                    slot_dim = described.slots[param].dim
                if slot_unit is None:
                    # No unit stated for this slot, so units cannot carry the
                    # edge; dimensions can. pH into a pOH slot and a ratio into
                    # a coefficient slot are sound and state no unit at all.
                    if not unit and slot_dim is not None and source_dim is not None \
                            and slot_dim == source_dim:
                        graph[source].append((target, index, 1.0))
                    continue
                factor = convert(unit, slot_unit)
                if factor is None:
                    continue
                if slot_dim is not None and source_dim is not None and \
                        slot_dim != source_dim and (slot_dim, source_dim) not in REFINES:
                    notes.append(
                        f"{source}({source_dim}) -> {target}.{param}({slot_dim}) "
                        f"agrees on unit {slot_unit} but not on dimension")
                    continue
                graph[source].append((target, index, factor))
    return graph, answers, notes


def path(graph, source, target, calls):
    """Whether `calls` calls can start at `source` and end at `target`.

    Repeats are allowed: a relation may feed itself, which is how a two-step
    row that only converts units reads.
    """
    if calls == 1:
        return source == target
    frontier = {source}
    for _ in range(calls - 1):
        nxt = set()
        for node in frontier:
            for (target_name, _slot, _factor) in graph.get(node, ()):
                nxt.add(target_name)
        if target in nxt:
            return True
        frontier = nxt
    return False


def report(graph, answers, notes):
    chainable = sum(1 for ok, _u in answers.values() if ok)
    edges = sum(len(v) for v in graph.values())
    print(f"relations in the catalogue             {len(RELATIONS)}")
    print(f"answers that are a single number       {chainable}")
    print(f"relations with a declared slot unit    {len(load_slot_units())}")
    print(f"composition edges                      {edges}")
    if notes:
        print(f"\ncross-check disagreements               {len(notes)}")
        for note in notes[:10]:
            print(f"  {note}")
    carried = Counter()
    for source, outs in graph.items():
        carried[answers[source][1]] += len(outs)
    print("\nedges by the unit a chain carries:")
    for unit, count in carried.most_common(12):
        print(f"  {(unit or '(none)'):<14} {count}")
    landing = Counter(target for outs in graph.values()
                      for (target, _s, _f) in outs)
    print("\nthe relations a chain can land on most often:")
    for name, count in landing.most_common(10):
        print(f"  {name:<24} {count} sources")
    sources = sorted(n for n, outs in graph.items() if outs)
    print(f"\nrelations that can open a chain: {len(sources)}")
    print("\ncalls needed to reach a relation from the busiest source")
    best = max(graph, key=lambda n: len(graph[n]))
    for calls in range(1, MAX_CALLS + 1):
        reach = {n for n in RELATIONS if path(graph, best, n, calls)}
        print(f"  {calls} call(s) from {best}: {len(reach)} relations")


def measure_rows(graph, rows_path):
    """Score the labelled bank rows against the graph.

    Each row is `id first needed calls verdict note`. The graph's answer and the
    read's answer are printed side by side, because they are different claims:
    the graph says a chain of that length is *unit- and dimension-compatible*,
    and the read says the row's own arithmetic is that chain. When they disagree
    the row is where the graph's limit shows: it checks units, never roles.
    """
    rows = []
    with open(rows_path, encoding="utf-8") as handle:
        for line in handle:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            rows.append(line.split())
    print(f"\nlabelled rows: {len(rows)}")
    reads = Counter()
    agree = 0
    for row in rows:
        row_id, first, needed, calls, verdict, note = row
        read_calls = None if calls == "-" else int(calls)
        graph_calls = None
        if first != "-" and needed != "-":
            for n in range(1, MAX_CALLS + 1):
                if path(graph, first, needed, n):
                    graph_calls = n
                    break
        reads[verdict] += 1
        if graph_calls == read_calls:
            agree += 1
        print(f"  {row_id:<12} {first:<22} -> {needed:<19} "
              f"read={calls:<2} graph={graph_calls if graph_calls else '-':<2} "
              f"{verdict:<8} {note}")
    print()
    for key, count in reads.most_common():
        print(f"  {key:<10} {count}")
    print(f"  graph agrees with the read on {agree} of {len(rows)} rows")


def main():
    parser = argparse.ArgumentParser(description="Composition graph")
    parser.add_argument("--rows", help="a labelled row file to measure")
    args = parser.parse_args()
    graph, answers, notes = build_graph()
    report(graph, answers, notes)
    if args.rows:
        measure_rows(graph, args.rows)


if __name__ == "__main__":
    main()
