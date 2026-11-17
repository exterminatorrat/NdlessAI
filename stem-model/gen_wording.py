"""Generate a chemistry wording corpus and the ledger that says how wide it is.

Rows come from `surface/spec.py` (what each slot is), `surface/grammar.py` (how a
sentence is composed) and `surface/lexicon.py` (what a student calls it). The
corpus goes in its own fixture file so before-and-after is measurable on the same
eval rows, and the ledger counts *structures* rather than rows, because a
relation with four hundred rows from three structures is three.

    python gen_wording.py --write          # the corpus and the ledger
    python gen_wording.py --check          # fail when either is stale

Every row is checked before it is written: it solves, every argument is grounded
in its own text by the runtime's rule, its structure id is stable, and a reader
could resolve it. Rows refused for resolvability are counted in the ledger
against the relation, with the reason, rather than dropped silently.
"""

import argparse
import glob
import json
import os
import random
import re
import sys

import subprocess
import sys

from gen_traces import render_call
from relations import RELATIONS
from surface import grammar, lexicon, numbers, spec

CORPUS = "fixtures/paraphrase_chem-generated_r6.jsonl"
LEDGER = "../docs/wording-ledger.md"
EXISTING = "fixtures/paraphrase_*.jsonl"

# The eight measured confusion pairs. Both sides are rendered with the *same*
# numbers and the same structure, so the only difference between the two rows is
# the word that decides the relation. A pair that cannot be decided by wording is
# recorded as such instead of being faked: `carnot` and `carnot_efficiency` return
# one quantity in two formats, which is ELV-77's contract defect.
CONTRAST = (
    ("sin_eval", "cos_eval"),
    ("fahrenheit_to_celsius", "celsius_to_fahrenheit"),
    ("geom_seq", "arith_seq"),
    ("percent_change", "percent_error"),
    ("calorimetry_c", "calorimetry_mass"),
    ("h_from_ph", "ph"),
    ("dilution", "dilution_v1"),
    ("carnot_efficiency", "carnot"),
)

UNRESOLVABLE_PAIRS = {
    ("carnot_efficiency", "carnot"):
        "same quantity in two formats, no wording decides it (ELV-77)",
}

EVAL_IN = 5          # one structure in five is held out whole
PER_RELATION = 24    # structures sampled per relation, eval structures first
ARG_SETS = 1
JACCARD_LIMIT = 0.9

NUMBER = re.compile(r"-?\d+(?:\.\d+)?")


def tokens(text):
    return {match.group(0) for match in NUMBER.finditer(text)}


def grounded(value, text):
    """The runtime's rule: the value's digits appear on a full number token.

    Ported from `pipeline_args_grounded` in `runtime/pipeline.c`: 0 and 1 are
    exempt, everything else has to appear as written, sign included.
    """
    if value in (0, 1):
        return True
    text_of = numbers.canon(value)
    return text_of is not None and text_of in tokens(text)


def normalised(text):
    """The words of a sentence, numbers dropped: what two rows share in wording."""
    return {word for word in re.findall(r"[a-z]+", text.lower()) if len(word) > 2}


def jaccard(left, right):
    if not left or not right:
        return 0.0
    return len(left & right) / len(left | right)


def existing_words():
    """Normalised word sets per relation, so a new row is not a near-duplicate."""
    seen = {}
    for path in sorted(glob.glob(EXISTING)):
        if os.path.abspath(path) == os.path.abspath(CORPUS):
            continue
        with open(path) as handle:
            for line in handle:
                row = json.loads(line)
                seen.setdefault(row["relation"], []).append(
                    normalised(row["question"]))
    return seen


def held_out(relation, structure_id):
    """One structure in five, by hash of the relation and the structure id."""
    digest = sum(ord(char) * (index + 1)
                 for index, char in enumerate(f"{relation}|{structure_id}"))
    return digest % EVAL_IN == 0


def tidy(values):
    """Integral floats as integers: the corpus checker grounds `str(arg)`, so 87.0
    has to be written 87.0 and 87 has to be written 87."""
    return [int(value) if isinstance(value, float) and value.is_integer() else value
            for value in values]


def arg_sets(name, count, seed):
    """Argument tuples the relation's own generator produces, so every row solves."""
    relation = RELATIONS[name]
    rng = random.Random(f"{name}|{seed}")
    sets = []
    tries = 0
    while len(sets) < count and tries < count * 20:
        tries += 1
        values, _ = relation.gen(rng)
        values = tidy(values)
        if all(numbers.canon(value) is not None for value in values):
            sets.append(values)
    return sets


def build(seed=1, per_relation=PER_RELATION):
    """Yield (rows, ledger) for every chemistry spec, in a stable order."""
    known = existing_words()
    rows, ledger = [], []
    for name in sorted(spec.SPECS):
        if name not in RELATIONS:
            continue
        relation_spec = spec.SPECS[name]
        all_structures = list(grammar.structures(relation_spec))
        refused = [s for s in all_structures if grammar.refusal(relation_spec, s)]
        usable = [s for s in all_structures if not grammar.refusal(relation_spec, s)]
        eval_structures = [s for s in usable if held_out(name, s.id)]
        train_structures = [s for s in usable if not held_out(name, s.id)]
        budget = max(per_relation, len(eval_structures))
        sampled = eval_structures + train_structures[:max(0, budget - len(eval_structures))]
        sets = arg_sets(name, ARG_SETS, seed)
        made = 0
        for index, structure in enumerate(sampled):
            for args in sets:
                register = lexicon.REGISTERS[index % len(lexicon.REGISTERS)]
                rendered = grammar.render(relation_spec, args, structure, register, index)
                if rendered is None:
                    continue
                text = rendered.text
                if not all(grounded(value, text) for value in args):
                    continue
                words = normalised(text)
                if any(jaccard(words, other) > JACCARD_LIMIT
                       for other in known.get(name, ())):
                    continue
                RELATIONS[name].solve(*args)
                rows.append({
                    "relation": name,
                    "args": args,
                    "question": text,
                    "split": "eval" if held_out(name, structure.id) else "train",
                    "structure": structure.id,
                })
                made += 1
        ledger.append({
            "relation": name,
            "slots": len(relation_spec.slots),
            "same_dim": relation_spec.same_dim,
            "structures": len(all_structures),
            "refused": len(refused),
            "reason": grammar.refusal(relation_spec, refused[0]) if refused else None,
            "rows": made,
            "eval_rows": sum(1 for row in rows if row["relation"] == name
                             and row["split"] == "eval"),
        })
    return rows, ledger


def contrast_rows(seed, per_pair=3):
    """Rows for the measured confusion pairs, both sides on the same numbers."""
    made, notes = [], []
    rng = random.Random(f"contrast|{seed}")
    for left, right in CONTRAST:
        if (left, right) in UNRESOLVABLE_PAIRS:
            notes.append({"pair": f"{left}/{right}", "rows": 0,
                          "note": UNRESOLVABLE_PAIRS[(left, right)]})
            continue
        if left not in spec.SPECS or right not in spec.SPECS:
            continue  # reported under the deferred pairs, with the reason
        left_spec, right_spec = spec.SPECS[left], spec.SPECS[right]
        if len(left_spec.slots) != len(right_spec.slots):
            notes.append({"pair": f"{left}/{right}", "rows": 0,
                          "note": f"arity differs ({len(left_spec.slots)} against "
                                  f"{len(right_spec.slots)})"})
            continue
        shared = [structure for structure in grammar.structures(left_spec)
                  if structure.names_roles
                  and not grammar.refusal(left_spec, structure)
                  and any(other.id == structure.id for other in grammar.structures(right_spec))]
        if not shared:
            notes.append({"pair": f"{left}/{right}", "rows": 0,
                          "note": "no structure is valid for both sides"})
            continue
        chosen = shared[:per_pair]
        args = None
        for _ in range(60):
            values = tidy(RELATIONS[left].gen(rng)[0])
            if not all(numbers.canon(value) is not None for value in values):
                continue
            try:
                RELATIONS[right].solve(*values)
            except Exception:  # noqa: BLE001 - a domain mismatch is a skipped draw
                continue
            args = values
            break
        if args is None:
            notes.append({"pair": f"{left}/{right}", "rows": 0,
                          "note": "no argument tuple is valid for both relations"})
            continue
        count = 0
        for index, structure in enumerate(chosen):
            for name, entry in ((left, left_spec), (right, right_spec)):
                other = next(s for s in grammar.structures(entry) if s.id == structure.id)
                register = lexicon.REGISTERS[index % len(lexicon.REGISTERS)]
                rendered = grammar.render(entry, args, other, register, index)
                if rendered is None:
                    continue
                text = rendered.text
                if not all(grounded(value, text) for value in args):
                    continue
                RELATIONS[name].solve(*args)
                made.append({
                    "relation": name,
                    "args": list(args),
                    "question": text,
                    "split": "train",
                    "structure": structure.id,
                    "pair": f"{left}/{right}",
                })
                count += 1
        notes.append({"pair": f"{left}/{right}", "rows": count, "note": ""})
    return made, notes


def deferred_pairs():
    """Measured confusion pairs this corpus cannot yet carry, with the reason."""
    out = []
    for left, right in CONTRAST:
        pair = f"{left}/{right}"
        if (left, right) in UNRESOLVABLE_PAIRS:
            continue
        if left not in spec.SPECS or right not in spec.SPECS:
            missing = [name for name in (left, right) if name not in spec.SPECS]
            out.append((pair, "no spec for " + ", ".join(f"`{name}`" for name in missing)
                        + " — maths relations, deferred to the subject phasing"))
    return out


def ledger_text(ledger, contrast=()):
    total_structures = sum(entry["structures"] for entry in ledger)
    # The header counts the file, so paired contrast rows are included even though
    # they are reported per pair rather than per relation.
    total_rows = (sum(entry["rows"] for entry in ledger)
                  + sum(entry["rows"] for entry in contrast))
    total_refused = sum(entry["refused"] for entry in ledger)
    total_eval = sum(entry["eval_rows"] for entry in ledger)
    subjects = sorted({RELATIONS[entry["relation"]].subject for entry in ledger})
    lines = [
        "# Wording ledger — " + ", ".join(subjects),
        "",
        "Generated by `stem-model/gen_wording.py`; `--check` fails when it is stale.",
        "A structure is a (given clause, ask clause, framing) triple, so the count is",
        "the number of distinct sentence shapes a relation admits, not the number of",
        "rows drawn from them.",
        "",
        f"{len(ledger)} relations, {total_structures} structures, {total_rows} rows "
        f"({total_eval} eval), {total_refused} structures refused as unresolvable.",
        "",
        "| relation | slots | same-dimension pairs | structures | refused | rows | eval rows |",
        "| --- | --- | --- | --- | --- | --- | --- |",
    ]
    for entry in ledger:
        pairs = "; ".join("/".join(pair) for pair in entry["same_dim"]) or "-"
        lines.append(f"| `{entry['relation']}` | {entry['slots']} | {pairs} | "
                     f"{entry['structures']} | {entry['refused']} | {entry['rows']} | "
                     f"{entry['eval_rows']} |")
    deferred = deferred_pairs()
    if contrast:
        lines += [
            "",
            "## Contrast pairs",
            "",
            "Both sides are rendered on the same numbers and the same structure, so the",
            "only difference is the word that decides the relation.",
            "",
            "| pair | rows | note |",
            "| --- | --- | --- |",
        ]
        for entry in contrast:
            lines.append(f"| `{entry['pair']}` | {entry['rows']} | {entry['note']} |")
    if os.path.exists(CONTRAST_OUT):
        with open(CONTRAST_OUT) as handle:
            rows = [json.loads(line) for line in handle if line.strip()]
        per_pair = {}
        for row in rows:
            per_pair.setdefault(row["pair"], []).append(row)
        missed_by = sorted({row.get("missed_by", "?") for row in rows})
        lines += [
            "",
            "## Contrast corpus (filtered by the model)",
            "",
            f"`{CONTRAST_OUT}` — {len(rows)} rows, kept only where the current",
            f"weights ({', '.join(missed_by)}) get the call wrong, so every row is a",
            "hard case rather than a pair that merely looks similar on paper. Each pair",
            "uses one argument tuple across both sides, so the only difference between",
            "the two rows is the word that decides the relation. Regenerating it needs",
            "weights, which is why it is committed as a filtered snapshot rather than",
            "regenerated by `--check`.",
            "",
            "| pair | rows |",
            "| --- | --- |",
        ]
        for pair, side_rows in sorted(per_pair.items()):
            lines.append(f"| `{pair}` | {len(side_rows)} |")
    if spec.BLOCKED_SPECIES:
        lines += [
            "",
            "## Relations where a species would contradict the call",
            "",
            "The formula-led frame names a substance. It is offered only where the name",
            "cannot change the answer, because the alternative is a row whose species",
            "contradicts the numbers it prints — a molar mass the call takes as an",
            "argument, a Ka, a coefficient. These are the rest, with the reason.",
            "",
            "| relation | why no species |",
            "| --- | --- |",
        ]
        for name, reason in sorted(spec.BLOCKED_SPECIES.items()):
            lines.append(f"| `{name}` | {reason} |")
    if grammar.DEFERRED_SHAPES:
        lines += [
            "",
            "## Shapes this generator does not carry, and why",
            "",
            "| shape | why |",
            "| --- | --- |",
        ]
        for shape, reason in grammar.DEFERRED_SHAPES.items():
            lines.append(f"| {shape} | {reason} |")
    if deferred:
        lines += [
            "",
            "## Pairs with no contrast rows, and why",
            "",
            "| pair | why no contrast rows |",
            "| --- | --- |",
        ]
        for pair, reason in deferred:
            lines.append(f"| `{pair}` | {reason} |")
    lines += [
        "",
        "## Provenance",
        "",
        "Which registers each spec's rows are written in, and where the structural idea",
        "came from. Non-commercial material is read for structure and register only: no",
        "sentence is copied, every row is authored from a spec, and no row reproduces a",
        "phrase from a source. A relation absent from this table was authored in this",
        "repository from its own contract.",
        "",
        "| relation | registers | structural idea from |",
        "| --- | --- | --- |",
    ]
    sourced = [entry for entry in ledger if entry["relation"] in spec.PROVENANCE]
    for entry in sourced:
        registers, key = spec.PROVENANCE[entry["relation"]]
        lines.append(f"| `{entry['relation']}` | {registers} | `{key}` |")
    lines.append(f"| the other {len(ledger) - len(sourced)} relations | "
                 f"{spec.DEFAULT_PROVENANCE[0]} | `{spec.DEFAULT_PROVENANCE[1]}` |")
    lines += ["", "Sources, by key:", ""]
    for key, text in spec.SOURCES.items():
        lines.append(f"- `{key}` — {text}.")
    lines += [
        "",
        "## Refusals, with the reason",
        "",
        "A structure is refused only when nothing in the text settles which slot is",
        "which: two slots of the relation share a dimension and the given block does",
        "not name roles. That is a property of the relation's slots rather than of the",
        "wording, so the fraction is reported here and not in a pull request body.",
        "",
    ]
    refused = [entry for entry in ledger if entry["refused"]]
    if refused:
        for entry in refused:
            share = entry["refused"] / entry["structures"]
            lines.append(f"- `{entry['relation']}` — {entry['refused']} of "
                         f"{entry['structures']} structures ({share:.0%}): {entry['reason']}")
    else:
        lines.append("- none")
    return "\n".join(lines) + "\n"


def all_rows(seed, per_relation=PER_RELATION):
    """The whole corpus: one row set per relation, then the paired contrast rows."""
    rows, ledger = build(seed, per_relation)
    extra, notes = contrast_rows(seed)
    return rows + extra, ledger, notes


def write(seed):
    rows, ledger, notes = all_rows(seed)
    with open(CORPUS, "w") as handle:
        for row in rows:
            handle.write(json.dumps(row, ensure_ascii=False) + "\n")
    with open(LEDGER, "w") as handle:
        handle.write(ledger_text(ledger, notes))
    return rows, ledger


def cohort(ceiling, weights, binary, seq, max_new):
    """Hardness of the held-out structures, through the model.

    The eval side of the corpus is held out by structure hash, so it measures
    structures the model has never trained on. A corpus whose held-out
    structures are read perfectly has stopped containing structures it has not
    seen — it has drifted back toward memorised wording — and this is the only
    place in the pipeline that would notice.
    """
    if not os.path.exists(weights) or not os.path.exists(binary):
        print(f"cannot run here: missing {weights if not os.path.exists(weights) else binary}")
        return 77
    tokenizer_path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                  "data", "tokenizer.json")
    if not os.path.exists(tokenizer_path):
        print("cannot run here: no data/tokenizer.json")
        return 77
    import json as _json

    from tokenizer import EOS, Tokenizer

    tokenizer = Tokenizer.from_json(_json.load(open(tokenizer_path)))
    rows, _, _ = all_rows(1)
    held = [row for row in rows if row["split"] == "eval"]
    exact = 0
    for row in held:
        ids = tokenizer.encode(f"{row['question']} <sep>")
        env = {"NSM_GEN": str(max_new), "NSM_SEQ": str(seq), "PATH": "/usr/bin:/bin"}
        out = subprocess.run([binary, weights, *map(str, ids)],
                             capture_output=True, text=True, env=env).stdout
        generated = [int(x) for line in out.splitlines() if line.startswith("gen ")
                     for x in line.split()[1:]]
        got = tokenizer.decode([token for token in generated if token != EOS]).strip()
        exact += got == render_expected(row)
    share = exact / len(held) if held else 1.0
    print(f"{len(held)} held-out structures: {exact} exact, {share:.1%}")
    if share > ceiling:
        print(f"too easy: {share:.1%} is above the {ceiling:.0%} drift ceiling")
        return 1
    return 0


SHINGLE = 7


def shingles(text, size=SHINGLE):
    """Overlapping word windows, so a copied run of words is visible at any offset.

    The window is seven words because the rule allows six: a match this finds is
    already a violation. Numbers are dropped before windowing — "4.18 J/(g C)" is
    a value two questions can share without either having been copied, and the
    licence is about text.
    """
    words = re.findall(r"[a-z]+", text.lower())
    return {" ".join(words[index:index + size])
            for index in range(max(0, len(words) - size + 1))}


def source_guard(directory, rows):
    """Fail when a generated row reproduces six consecutive words of a source.

    The licensing rule allows non-commercial text to be read for structure and
    register, never copied. Sources are not kept in the repository, so this runs
    against whatever local corpus is named — on this machine, the College Board
    bank that already had a "local and gitignored" rule of its own.
    """
    if not os.path.isdir(directory):
        print(f"cannot run here: {directory} is not a directory")
        return 77
    source = set()
    for path in sorted(glob.glob(os.path.join(directory, "*"))):
        if not os.path.isfile(path):
            continue
        try:
            with open(path, errors="ignore") as handle:
                for line in handle:
                    source |= shingles(line)
        except OSError:
            continue
    if not source:
        print(f"cannot run here: no readable text under {directory}")
        return 77
    worst = 0
    flagged = 0
    for row in rows:
        shared = source & shingles(row["question"])
        worst = max(worst, len(shared))
        if shared:
            flagged += 1
            print(f"{row['relation']}: {sorted(shared)}")
    print(f"{len(rows)} rows against {len(source)} source windows of "
          f"{SHINGLE} words; {flagged} rows over the six-word limit, "
          f"worst {worst} runs")
    return 1 if worst else 0


CONTRAST_OUT = "fixtures/paraphrase_contrast_r7.jsonl"


def keep_only_misses(rows, weights, binary, seq, max_new):
    """Run the current weights over candidate rows and keep the ones it fails.

    A minimal pair is only worth training on if the model actually cannot tell the
    two sides apart. A pair that merely looks similar on paper teaches nothing, so
    the filter is the model's own error rather than my judgement of it.
    """
    import json as _json

    from tokenizer import EOS, Tokenizer

    tokenizer_path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                  "data", "tokenizer.json")
    tokenizer = Tokenizer.from_json(_json.load(open(tokenizer_path)))
    kept, tried = [], 0
    for row in rows:
        tried += 1
        ids = tokenizer.encode(f"{row['question']} <sep>")
        env = {"NSM_GEN": str(max_new), "NSM_SEQ": str(seq), "PATH": "/usr/bin:/bin"}
        out = subprocess.run([binary, weights, *map(str, ids)],
                             capture_output=True, text=True, env=env).stdout
        generated = [int(x) for line in out.splitlines() if line.startswith("gen ")
                     for x in line.split()[1:]]
        got = tokenizer.decode([token for token in generated if token != EOS]).strip()
        if got != render_expected(row):
            row = dict(row, missed_by=weights)
            kept.append(row)
    return kept, tried


def render_expected(row):
    return render_call(row["relation"], row["args"])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--write", action="store_true", help="rewrite the corpus and the ledger")
    ap.add_argument("--check", action="store_true", help="fail when either file is stale")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--per-relation", type=int, default=PER_RELATION)
    ap.add_argument("--contrast-out", default=None, metavar="PATH",
                    help="write the contrast rows the current weights get wrong")
    ap.add_argument("--source-guard", default=None, metavar="DIR",
                    help="fail when a row reproduces six words of a source corpus")
    ap.add_argument("--cohort", action="store_true",
                    help="measure how hard the held-out structures still are")
    ap.add_argument("--ceiling", type=float, default=0.75,
                    help="the drift ceiling for --cohort")
    ap.add_argument("--weights", default="weights.bin")
    ap.add_argument("--binary", default="runtime/infer_host")
    ap.add_argument("--seq", type=int, default=192)
    ap.add_argument("--max-new", type=int, default=24)
    args = ap.parse_args()

    if args.contrast_out:
        made, notes = contrast_rows(args.seed, per_pair=6)
        if not made:
            print("no contrast candidates: no pair has both sides spec'd")
            return 1
        kept, tried = keep_only_misses(made, args.weights, args.binary,
                                       args.seq, args.max_new)
        # A pair with one side left is not a contrast: the row teaches nothing
        # without the sentence that differs from it by one word.
        present = {}
        for row in kept:
            present.setdefault(row["pair"], set()).add(row["relation"])
        kept = [row for row in kept
                if len(present[row["pair"]]) == len(row["pair"].split("/"))]
        with open(args.contrast_out, "w") as handle:
            for row in kept:
                handle.write(json.dumps(row, ensure_ascii=False) + "\n")
        per_pair = {}
        for row in kept:
            per_pair[row["pair"]] = per_pair.get(row["pair"], 0) + 1
        print(f"{len(kept)} of {tried} candidate rows are missed by {args.weights}")
        for pair, count in sorted(per_pair.items()):
            print(f"  {pair:44s}{count:4d}")
        return 0

    if args.source_guard:
        return source_guard(args.source_guard, all_rows(args.seed, args.per_relation)[0])

    if args.cohort:
        return cohort(args.ceiling, args.weights, args.binary, args.seq, args.max_new)

    rows, ledger, notes = all_rows(args.seed, args.per_relation)
    text = ledger_text(ledger, notes)
    if args.check:
        stale = []
        if not os.path.exists(CORPUS):
            stale.append(CORPUS)
        else:
            with open(CORPUS) as handle:
                written = [json.loads(line) for line in handle if line.strip()]
            if written != rows:
                stale.append(CORPUS)
        if not os.path.exists(LEDGER):
            stale.append(LEDGER)
        else:
            with open(LEDGER) as handle:
                if handle.read() != text:
                    stale.append(LEDGER)
        if stale:
            print("stale: " + ", ".join(stale))
            print("run: python gen_wording.py --write")
            return 1
        print(f"ledger and corpus are current: {len(rows)} rows, "
              f"{sum(entry['structures'] for entry in ledger)} structures")
        return 0
    if args.write:
        write(args.seed)
        print(f"wrote {CORPUS} ({len(rows)} rows) and {LEDGER}")
        return 0
    print(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
