"""Score the model on wording it never saw.

Two banks of paraphrases feed this:

- stress_cases.CASES, 24 hand-written problems, the quick check.
- fixtures/paraphrase_cases.jsonl, five phrasings for every registered
  relation, tagged train or eval. The two train rows per relation are the ones
  whose sha256("relation|question") sorts lowest, so the split is a fixed rule
  rather than a judgement call, and the eval rows are the only honest
  measurement of wording robustness.

Every corpus row is checked before it is scored: the relation must accept its
args, and every arg must be findable in the question under the runtime's own
grounding rule (pipeline_args_grounded in runtime/pipeline.c, mirrored below),
because the device refuses an answer whose arguments are not in the text.

The corpus check needs no data/, no weights and no binary, so this script
still does something useful in a bare checkout: it validates the corpus and
says which artifact is missing. Pass --check-corpus to stop there.
"""

import argparse
import glob
import json
import os
import subprocess

from gen_traces import render_call
from relations import RELATIONS
from stress_cases import CASES
from tokenizer import EOS, Tokenizer

CORPUS = os.path.join("fixtures", "paraphrase_*.jsonl")


def is_digit(c):
    return "0" <= c <= "9"


def arg_in_text(arg, question):
    """Mirror of arg_in_text in runtime/pipeline.c.

    A number in the text grounds an argument when the digits match on a full
    token and the sign agrees: a negative argument needs a minus directly
    before the digits, a positive one needs an occurrence without one. 0 and 1
    are always accepted because polynomials write those coefficients
    implicitly.
    """

    text = str(arg)
    negative = text.startswith("-")
    digits = text[1:] if negative else text
    if not digits:
        return False
    if digits == "0" or digits == "1":
        return True

    start = 0
    while True:
        at = question.find(digits, start)
        if at < 0:
            return False
        start = at + 1
        end = at + len(digits)
        if end < len(question) and is_digit(question[end]):
            continue
        before = at
        has_minus = False
        while before > 0 and question[before - 1] == " ":
            before -= 1
        if before > 0 and question[before - 1] == "-":
            has_minus = True
        elif before > 0 and is_digit(question[before - 1]):
            continue
        if has_minus == negative:
            return True


def load_corpus(path):
    if not os.path.exists(path):
        raise SystemExit(f"{path} missing")

    rows = []
    with open(path) as fh:
        for lineno, line in enumerate(fh, 1):
            if not line.strip():
                continue
            row = json.loads(line)
            where = f"{path}:{lineno}"
            name = row.get("relation")
            if name not in RELATIONS:
                raise SystemExit(f"{where}: unknown relation {name!r}")
            rel = RELATIONS[name]
            args = row["args"]
            if len(args) != len(rel.params):
                raise SystemExit(
                    f"{where}: {name} takes {len(rel.params)} args, got {len(args)}")
            try:
                rel.solve(*args)
            except Exception as exc:  # noqa: BLE001
                raise SystemExit(f"{where}: {name}{args} solve raised {exc!r}") from exc
            missing = [a for a in args if not arg_in_text(a, row["question"])]
            if missing:
                raise SystemExit(f"{where}: {name}{args} args not in the text: {missing}")
            if row.get("split") not in ("train", "eval"):
                raise SystemExit(f"{where}: split must be train or eval")
            rows.append(row)
    return rows


def load_corpora(pattern):
    """Every corpus file matching the pattern, validated row by row.

    One file per authoring stream keeps parallel work from colliding in one
    file; the evaluator reads them as one corpus, so the split and the totals
    are still computed over everything.
    """
    paths = sorted(glob.glob(pattern))
    if not paths:
        raise SystemExit(f"{pattern} matched no corpus files")
    rows = []
    for path in paths:
        rows.extend(load_corpus(path))
    print(f"corpus files: {', '.join(paths)}")
    return rows


def run_case(binary, weights, tok, seq, max_new, name, params, question):
    """One question through the binary; returns (kind, expected, got)."""

    expected = render_call(name, params)
    ids = tok.encode(f"{question} <sep>")
    env = {"NSM_GEN": str(max_new), "NSM_SEQ": str(seq), "PATH": "/usr/bin:/bin"}
    out = subprocess.run([binary, weights, *map(str, ids)],
                         capture_output=True, text=True, env=env).stdout
    gen = []
    for line in out.splitlines():
        if line.startswith("gen "):
            gen = [int(x) for x in line.split()[1:]]
            break
    got = tok.decode([g for g in gen if g != EOS]).strip()
    if got == expected:
        return "exact", expected, got
    if got.split(" ")[:2] == expected.split(" ")[:2]:
        return "args", expected, got
    return "relation", expected, got


def summarise(label, cases, max_show, run):
    print(f"\n{label}: {len(cases)} cases")
    results = []
    for name, params, question in cases:
        kind, expected, got = run(name, params, question)
        results.append((kind, expected, got, question))
    for kind, heading in (("exact", "exact call"),
                          ("args", "right relation, wrong args"),
                          ("relation", "wrong relation")):
        hits = sum(1 for result in results if result[0] == kind)
        print(f"  {heading:<28}{hits:5d}  {hits / len(results):6.1%}")
    misses = [result for result in results if result[0] != "exact"]
    for _, expected, got, question in misses[:max_show]:
        print(f"\n  Q: {question}\n     want {expected}\n     got  {got}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--weights", default="weights.bin")
    ap.add_argument("--binary", default="runtime/infer_host")
    ap.add_argument("--seq", type=int, default=160)
    ap.add_argument("--max-new", type=int, default=16)
    ap.add_argument("--corpus", default=CORPUS)
    ap.add_argument("--max-show", type=int, default=5,
                    help="failures to print per split")
    ap.add_argument("--check-corpus", action="store_true",
                    help="validate the corpus and stop, no model needed")
    ap.add_argument("--no-stress", action="store_true",
                    help="skip the 24 hand-written stress cases")
    args = ap.parse_args()

    corpus = load_corpora(args.corpus)
    splits = [s for s in ("train", "eval") if any(r["split"] == s for r in corpus)]
    print(f"{len(corpus)} cases over {len(RELATIONS)} relations")
    for split in splits:
        print(f"  {split:<6}{sum(1 for r in corpus if r['split'] == split):5d}")
    print("  every row solves and every argument is grounded in its question")

    if args.check_corpus:
        return 0

    missing = [p for p in (args.binary, args.weights, "data/tokenizer.json")
               if not os.path.exists(p)]
    if missing:
        print(f"\nno model run: missing {', '.join(missing)}")
        print("corpus validation is the only check available here")
        return 0

    with open("data/tokenizer.json") as fh:
        tok = Tokenizer.from_json(json.load(fh))

    def run(name, params, question):
        return run_case(args.binary, args.weights, tok, args.seq, args.max_new,
                        name, params, question)

    if not args.no_stress:
        summarise("24 hand-written stress cases", CASES, args.max_show, run)

    for split in splits:
        summarise(f"{split} split",
                  [(r["relation"], r["args"], r["question"])
                   for r in corpus if r["split"] == split],
                  args.max_show, run)
    return 0


if __name__ == "__main__":
    main()
