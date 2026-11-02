import argparse
import json
import os
import random

from distractors import add_distractor
from gen_traces import render_call, run_call
from relations import RELATIONS
from tokenizer import Tokenizer, train as train_tokenizer


def build(relations, n, seed, distractor_rate):
    rng = random.Random(seed)
    rows = []
    for i in range(n):
        name = relations[i % len(relations)]
        rel = RELATIONS[name]
        params, question = rel.gen(rng)
        call = render_call(name, params)
        answer = run_call(call)
        if answer != rel.solve(*params):
            raise RuntimeError(f"{name} {params} round-trip mismatch")
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
    ap.add_argument("--vocab", type=int, default=512)
    ap.add_argument("--distractors", type=float, default=0.5)
    ap.add_argument("--seed", type=int, default=1)
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    names = sorted(RELATIONS)

    train_rows = build(names, args.train, args.seed, args.distractors)
    val_rows = build(names, args.val, args.seed + 90001, args.distractors)

    tok = train_tokenizer([f"{r['question']} <sep> {r['call']}" for r in train_rows],
                          args.vocab)

    for fname, rows in (("train.jsonl", train_rows), ("val.jsonl", val_rows)):
        with open(os.path.join(args.out, fname), "w") as f:
            for r in rows:
                f.write(json.dumps(r) + "\n")

    with open(os.path.join(args.out, "tokenizer.json"), "w") as f:
        json.dump(tok.to_json(), f)

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
    }
    with open(os.path.join(args.out, "meta.json"), "w") as f:
        json.dump(meta, f, indent=2)

    print(f"{meta['train']} train / {meta['val']} val rows -> {args.out}/")
    print(f"vocab {meta['vocab']}, {meta['with_distractor']} rows with a distractor")
    print(f"tokens: mean {meta['mean_tokens']:.1f}, max {meta['max_tokens']}; "
          f"call mean {meta['mean_call_tokens']:.1f}")


if __name__ == "__main__":
    main()
