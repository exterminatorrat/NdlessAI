import argparse
import json
import os
import subprocess

from tokenizer import EOS, Tokenizer


def run_c(binary, weights, ids, max_new, seq):
    env = {"NSM_GEN": str(max_new), "NSM_SEQ": str(seq), "PATH": "/usr/bin:/bin"}
    out = subprocess.run([binary, weights, *map(str, ids)],
                         capture_output=True, text=True, env=env).stdout
    for line in out.splitlines():
        if line.startswith("gen "):
            return [int(x) for x in line.split()[1:]]
    return []


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", default="data/val.jsonl")
    ap.add_argument("--tokenizer", default="data/tokenizer.json")
    ap.add_argument("--weights", default="weights.bin")
    ap.add_argument("--binary", default="runtime/infer_host")
    ap.add_argument("--limit", type=int, default=50)
    ap.add_argument("--max-new", type=int, default=16)
    ap.add_argument("--seq", type=int, default=128)
    args = ap.parse_args()

    if not os.path.exists(args.binary):
        raise SystemExit(f"{args.binary} missing; run `make host` first")

    tok = Tokenizer.from_json(json.load(open(args.tokenizer)))
    rows = [json.loads(line) for line in open(args.data)][:args.limit]

    exact = wrong = relation_ok = 0
    for r in rows:
        ids = tok.encode(f"{r['question']} <sep>")
        gen = run_c(args.binary, args.weights, ids, args.max_new, args.seq)
        got = tok.decode([g for g in gen if g != EOS]).strip()
        if got == r["call"]:
            exact += 1
        elif got.split()[:2] == r["call"].split()[:2]:
            relation_ok += 1
        else:
            wrong += 1

    n = len(rows)
    print(f"{n} held-out problems through the int8 C engine")
    print(f"  exact call        {exact:4d}  {exact / n:6.1%}")
    print(f"  relation, bad args{relation_ok:4d}  {relation_ok / n:6.1%}")
    print(f"  wrong relation    {wrong:4d}  {wrong / n:6.1%}")


if __name__ == "__main__":
    main()
