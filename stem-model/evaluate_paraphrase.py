import argparse
import os
import subprocess

from gen_traces import render_call
from relations import RELATIONS
from stress_cases import CASES


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--weights", default="weights.bin")
    ap.add_argument("--binary", default="runtime/infer_host")
    ap.add_argument("--seq", type=int, default=160)
    ap.add_argument("--max-new", type=int, default=16)
    args = ap.parse_args()

    if not os.path.exists(args.binary):
        raise SystemExit(f"{args.binary} missing; run `make -C runtime` first")

    from tokenizer import EOS, Tokenizer

    tok = Tokenizer.from_json(__import__("json").load(open("data/tokenizer.json")))
    exact = relation_ok = wrong = 0
    misses = []

    for name, params, question in CASES:
        expected = render_call(name, params)
        ids = tok.encode(f"{question} <sep>")
        env = {"NSM_GEN": str(args.max_new), "NSM_SEQ": str(args.seq),
               "PATH": "/usr/bin:/bin"}
        out = subprocess.run([args.binary, args.weights, *map(str, ids)],
                             capture_output=True, text=True, env=env).stdout
        gen = []
        for line in out.splitlines():
            if line.startswith("gen "):
                gen = [int(x) for x in line.split()[1:]]
                break
        got = tok.decode([g for g in gen if g != EOS]).strip()
        if got == expected:
            exact += 1
        elif got.split(" ")[:2] == expected.split(" ")[:2]:
            relation_ok += 1
            misses.append((question, expected, got))
        else:
            wrong += 1
            misses.append((question, expected, got))

    n = len(CASES)
    print(f"{n} paraphrased problems (wording never seen in training)")
    print(f"  exact call         {exact:3d}  {exact / n:6.1%}")
    print(f"  right relation     {relation_ok:3d}  {relation_ok / n:6.1%}")
    print(f"  wrong relation     {wrong:3d}  {wrong / n:6.1%}")
    for q, e, g in misses:
        print(f"\n  Q: {q}\n     want {e}\n     got  {g}")


if __name__ == "__main__":
    main()
