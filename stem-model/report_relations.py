"""Per-relation wording accuracy, so "the model is weak on wording" has a list.

The aggregate number hides which relations carry the deficit: hand-written
accuracy has sat between 71.8% and 73.8% across 84, 90, 102 and 136 relations,
which is what an average looks like when a handful of relations fail and most
are fine. This script prints the per-relation table for the corpus's honest
split (the `eval` rows, which the model never trains on) and can fail a build
when any relation with enough rows falls below a floor.

    python3 report_relations.py                  # the table
    python3 report_relations.py --floor 0.5      # exit 1 under the floor

The questions go through `runtime/main_device`, which is one process for the
whole batch and the same pipeline the calculator runs.
"""

import argparse
import collections
import json
import os
import subprocess
import sys

from evaluate_paraphrase import load_corpora
from gen_traces import render_call

DEFAULT_DATA = "data/held_out.jsonl"


def run_batch(binary, weights, seq, questions):
    """One process, one line per question: answer<TAB>call."""

    payload = "\n".join(questions) + "\n"
    env = {"PATH": "/usr/bin:/bin", "NSM_QUIET": "1"}
    out = subprocess.run([binary, weights, str(seq)], input=payload,
                         capture_output=True, text=True, env=env).stdout
    calls = []
    for line in out.splitlines():
        calls.append(line.partition("\t")[2])
    while len(calls) < len(questions):
        calls.append("")
    return calls[: len(questions)]


def report(rows, args):
    """rows: [(relation, question, expected)]"""

    calls = run_batch(args.binary, args.weights, args.seq,
                      [q for _, q, _ in rows])
    per = collections.defaultdict(lambda: [0, 0])
    misses = []
    for (relation, question, expected), got in zip(rows, calls):
        hit = got == expected
        per[relation][0] += 1
        per[relation][1] += hit
        if not hit:
            misses.append((relation, question, expected, got))

    total = sum(v[0] for v in per.values())
    exact = sum(v[1] for v in per.values())
    print(f"{total} questions over {len(per)} relations: "
          f"{exact / total:.1%} exact call")
    print()

    worst = sorted(per.items(), key=lambda kv: (kv[1][1] / kv[1][0], -kv[1][0]))
    print(f"{'relation':<24}{'correct':>8}{'of':>5}{'exact':>9}")
    thin = 0
    for relation, (n, hits) in worst:
        flag = ""
        if n >= args.min_rows and hits / n < args.floor:
            flag = "  below floor"
            thin += 1
        print(f"{relation:<24}{hits:>8}{n:>5}{hits / n:>9.1%}{flag}")

    print()
    print(f"{len(per)} relations, {thin} below {args.floor:.0%} on "
          f"{args.min_rows}+ rows")

    if args.show_misses:
        print()
        for relation, question, expected, got in misses[: args.show_misses]:
            print(f"  {relation}\n    Q: {question}\n"
                  f"    want {expected}\n    got  {got or '(nothing)'}")
    return thin


def corpus_rows(args):
    """The eval split of the paraphrase corpus, as (relation, question, call)."""

    rows = []
    for row in load_corpora(args.corpus):
        if row["split"] != args.split:
            continue
        rows.append((row["relation"], row["question"],
                     render_call(row["relation"], row["args"])))
    return rows


def held_out_rows(args):
    rows = []
    with open(args.data) as fh:
        for line in fh:
            row = json.loads(line)
            rows.append((row["relation"], row["question"], row["call"]))
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--source", choices=("held-out", "paraphrase"),
                    default="paraphrase",
                    help="held-out generated rows, or the hand-written corpus")
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--corpus", default="fixtures/paraphrase_*.jsonl")
    ap.add_argument("--split", default="eval",
                    help="corpus split to score: eval is the honest one")
    ap.add_argument("--weights", default="weights.bin")
    ap.add_argument("--binary", default="runtime/main_device")
    ap.add_argument("--seq", type=int, default=160)
    ap.add_argument("--floor", type=float, default=0.5)
    ap.add_argument("--max-thin", type=int, default=0,
                    help="fail when more than this many relations sit below "
                         "the floor; the gate's recorded budget")
    ap.add_argument("--min-rows", type=int, default=4,
                    help="a relation needs this many rows before the floor "
                         "judges it")
    ap.add_argument("--show-misses", type=int, default=0)
    ap.add_argument("--limit", type=int, default=0)
    args = ap.parse_args()

    for path in (args.binary, args.weights):
        if not os.path.exists(path):
            raise SystemExit(f"{path} missing; run `make -C runtime` first")

    rows = held_out_rows(args) if args.source == "held-out" else corpus_rows(args)
    if args.limit:
        rows = rows[: args.limit]
    if not rows:
        raise SystemExit("no rows to score")

    thin = report(rows, args)
    if thin > args.max_thin:
        print(f"\n{thin} relations below the floor, budget is {args.max_thin}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
