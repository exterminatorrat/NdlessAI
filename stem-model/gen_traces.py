import argparse
import json
import random

from distractors import add_distractor
from relations import RELATIONS


def render_call(name, args):
    return "CALL " + name + " " + " ".join(str(a) for a in args)


def run_call(text):
    parts = text.split()
    if len(parts) < 2 or parts[0] != "CALL":
        raise ValueError(f"bad call: {text}")
    name, raw = parts[1], parts[2:]
    if name not in RELATIONS:
        raise ValueError(f"unknown relation: {name}")
    rel = RELATIONS[name]
    if len(raw) != len(rel.params):
        raise ValueError(f"{name} takes {len(rel.params)} args, got {len(raw)}")
    args = []
    for token in raw:
        args.append(int(token) if token.lstrip("-").isdigit() else float(token))
    return rel.solve(*args)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--n", type=int, default=20)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--distractors", type=float, default=0.5)
    ap.add_argument("--out", default="traces.jsonl")
    args = ap.parse_args()

    rng = random.Random(args.seed)
    names = sorted(RELATIONS)
    rows = 0
    with open(args.out, "w") as out:
        for i in range(args.n):
            name = names[i % len(names)]
            rel = RELATIONS[name]
            params, question = rel.gen(rng)
            call = render_call(name, params)
            answer = run_call(call)
            assert answer == rel.solve(*params), f"{name} roundtrip mismatch"
            distractor = None
            if rng.random() < args.distractors:
                question, distractor = add_distractor(name, params, question, rng)
                assert str(distractor) in question, f"{name} distractor missing"
                assert str(distractor) not in {str(p) for p in params}, f"{name} distractor collides"
                assert run_call(call) == answer, f"{name} answer moved"
            out.write(json.dumps({
                "subject": rel.subject,
                "question": question,
                "call": call,
                "answer": answer,
                "distractor": distractor,
            }) + "\n")
            rows += 1

    print(f"{rows} traces -> {args.out}")
    print(f"{len(RELATIONS)} relations: {', '.join(names)}")
    with open(args.out) as f:
        for line in list(f)[:4]:
            row = json.loads(line)
            print(f"  {row['call']}\n    -> {row['answer']}")


if __name__ == "__main__":
    main()
