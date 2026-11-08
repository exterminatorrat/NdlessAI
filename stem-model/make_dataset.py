import argparse
import json
import os
import random
import sys
import time

from distractors import add_distractor
from gen_traces import render_call, run_call
from relations import RELATIONS
from surface import held_out_indices, render as render_phrasing, train_indices
from tokenizer import Tokenizer, VOCAB_SIZE, train as train_tokenizer

def build(relations, n, seed, distractor_rate, max_phrasings=None):
    return _build(relations, n, seed, distractor_rate, held_out=False,
                  max_phrasings=max_phrasings)


def build_held_out(relations, n, seed, distractor_rate):
    return _build(relations, n, seed, distractor_rate, held_out=True)


def _build(relations, n, seed, distractor_rate, held_out, max_phrasings=None):
    start = time.time()
    rng = random.Random(seed)
    rows = []
    for i in range(n):
        if i and i % 25000 == 0:
            rate = (time.time() - start) / i
            left = (n - i) * rate
            print(f"  {i}/{n} rows, {rate * 1000:.2f} ms/row, "
                  f"~{left / 60:.1f} min left", flush=True)
        name = relations[i % len(relations)]
        rel = RELATIONS[name]
        params, question = rel.gen(rng)
        pool = held_out_indices(name) if held_out else train_indices(name)
        if not held_out and max_phrasings:
            pool = pool[:max_phrasings]
        question = render_phrasing(name, params, rng.choice(pool))
        call = render_call(name, params)
        answer = run_call(call)
        got = call.split()[2:]
        if got != [str(p) for p in params]:
            raise RuntimeError(f"{name} {params} rendered as {call}")
        distractor = None
        if rng.random() < distractor_rate:
            question, distractor = add_distractor(name, params, question, rng)
            if str(distractor) in {str(p) for p in params}:
                raise RuntimeError(f"{name} distractor collides: {distractor}")
        rows.append({
            "relation": name,
            "subject": rel.subject,
            "question": question,
            "call": call,
            "answer": answer,
            "distractor": distractor,
        })
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="data")
    ap.add_argument("--train", type=int, default=200000)
    ap.add_argument("--val", type=int, default=4000)
    ap.add_argument("--held-out", type=int, default=2000,
                    help="rows using phrasings reserved from training")
    ap.add_argument("--vocab", type=int, default=VOCAB_SIZE,
                    help="vocabulary size (default %(default)s)")
    ap.add_argument("--tokenizer-rows", type=int, default=20000,
                    help="rows used to learn the BPE merges; more is slower "
                         "and changes the vocabulary almost not at all")
    ap.add_argument("--distractors", type=float, default=0.5)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--max-train-phrasings", type=int, default=0,
                    help="cap phrasings per relation; for A/B tests only")
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    names = sorted(RELATIONS)

    print(f"generating {args.train} train rows", flush=True)
    train_rows = build(names, args.train, args.seed, args.distractors,
                       args.max_train_phrasings or None)
    print(f"generating {args.val} val rows", flush=True)
    val_rows = build(names, args.val, args.seed + 90001, args.distractors)
    print(f"generating {args.held_out} held-out phrasing rows", flush=True)
    held_rows = build_held_out(names, args.held_out, args.seed + 31337, args.distractors)

    sample = train_rows[:args.tokenizer_rows]
    print(f"training tokenizer on {len(sample)} rows", flush=True)
    tok = train_tokenizer([f"{r['question']} <sep> {r['call']}" for r in sample],
                          args.vocab)

    print(f"writing {args.out}/train.jsonl and val.jsonl", flush=True)
    for fname, rows in (("train.jsonl", train_rows), ("val.jsonl", val_rows),
                        ("held_out.jsonl", held_rows)):
        with open(os.path.join(args.out, fname), "w") as f:
            for r in rows:
                f.write(json.dumps(r) + "\n")

    with open(os.path.join(args.out, "tokenizer.json"), "w") as f:
        json.dump(tok.to_json(), f)

    print("measuring token statistics", flush=True)
    lens = [len(tok.encode(f"{r['question']} <sep> {r['call']}")) for r in train_rows]
    calls = [len(tok.encode(r["call"])) for r in train_rows]
    meta = {
        "train": len(train_rows),
        "val": len(val_rows),
        "vocab": tok.size,
        "relations": names,
        "mean_tokens": sum(lens) / len(lens),
        "max_tokens": max(lens),
        "mean_call_tokens": sum(calls) / len(calls),
        "max_call_tokens": max(calls),
        "with_distractor": sum(1 for r in train_rows if r["distractor"] is not None),
        "seed": args.seed,
        "distractor_rate": args.distractors,
        "tokenizer_rows": len(sample),
        "held_out": len(held_rows),
        "train_phrasings": {n: len(train_indices(n)) for n in names},
        "held_out_phrasings": {n: len(held_out_indices(n)) for n in names},
    }
    with open(os.path.join(args.out, "meta.json"), "w") as f:
        json.dump(meta, f, indent=2)

    print(f"{meta['train']} train / {meta['val']} val rows -> {args.out}/")
    print(f"vocab {meta['vocab']}, {meta['with_distractor']} rows with a distractor")
    print(f"tokens: mean {meta['mean_tokens']:.1f}, max {meta['max_tokens']}; "
          f"call mean {meta['mean_call_tokens']:.1f}")


if __name__ == "__main__":
    main()
