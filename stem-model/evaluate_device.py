import argparse
import json
import os
import subprocess


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", default="data/val.jsonl")
    ap.add_argument("--weights", default="weights.bin")
    ap.add_argument("--binary", default="runtime/main_device")
    ap.add_argument("--limit", type=int, default=60)
    ap.add_argument("--seq", type=int, default=160)
    args = ap.parse_args()

    if not os.path.exists(args.binary):
        raise SystemExit(f"{args.binary} missing; run `make -C runtime` first")

    rows = [json.loads(line) for line in open(args.data)][:args.limit]
    text = "\n".join(r["question"] for r in rows) + "\n"
    out = subprocess.run([args.binary, args.weights, str(args.seq)],
                         input=text, capture_output=True, text=True,
                         env={"PATH": "/usr/bin:/bin", "NSM_QUIET": "1"}).stdout

    exact = bad_args = wrong = right_answer = 0
    for row, line in zip(rows, out.splitlines()):
        answer, _, call = line.partition("\t")
        if call == row["call"]:
            exact += 1
        elif call.split(" ")[:2] == row["call"].split(" ")[:2]:
            bad_args += 1
        else:
            wrong += 1
        if answer == row["answer"]:
            right_answer += 1

    n = len(rows)
    print(f"{n} held-out problems through the on-device path")
    print(f"  BPE -> fixed-point forward -> call -> C solver")
    print(f"  exact call           {exact:4d}  {exact / n:6.1%}")
    print(f"  relation, bad args   {bad_args:4d}  {bad_args / n:6.1%}")
    print(f"  wrong relation       {wrong:4d}  {wrong / n:6.1%}")
    print(f"  correct final answer {right_answer:4d}  {right_answer / n:6.1%}")


if __name__ == "__main__":
    main()
