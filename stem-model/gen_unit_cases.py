"""Correct calls, one "question<TAB>call" per line, for test_quantity --cases.

The unit check must never change a call that is right. Two sources say what
right is: the training builder (every phrasing, both wording variants and the
distractor sentence, exactly as make_dataset renders them) and every row of the
hand-written corpus, train and eval alike, because the eval rows are the
wording a student is closest to. Nothing is written to disk.

    python3 gen_unit_cases.py | runtime/test_quantity --cases
"""
import argparse
import glob
import json
import os
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ROOT)

from gen_traces import render_call  # noqa: E402
from make_dataset import _build  # noqa: E402
from relations import RELATIONS  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--per-relation", type=int, default=60,
                    help="generated rows per relation, per pool")
    args = ap.parse_args()

    names = sorted(RELATIONS)
    n = args.per_relation * len(names)
    rows = []
    for held_out, variant, seed in ((False, None, 11), (False, "train", 12),
                                    (False, "held_out", 13), (True, None, 14)):
        rows += _build(names, n, seed, distractor_rate=0.5, held_out=held_out,
                       variant=variant)
    out = [(r["question"], r["call"]) for r in rows]

    for path in sorted(glob.glob(os.path.join(ROOT, "fixtures",
                                              "paraphrase_*.jsonl"))):
        with open(path) as fh:
            for line in fh:
                if line.strip():
                    row = json.loads(line)
                    out.append((row["question"],
                                render_call(row["relation"], row["args"])))

    for question, call in out:
        print(f"{question}\t{call}")


if __name__ == "__main__":
    main()
