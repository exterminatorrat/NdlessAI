"""Fail if runtime/bpe.h was not generated from the tokenizer in use.

A stale bpe.h compiles fine, passes every unit test, and silently feeds the
model unfamiliar token ids. That has cost two debugging sessions, so it is
checked rather than trusted.
"""
import hashlib
import json
import os
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))


def source_sha(tokenizer_path):
    with open(tokenizer_path) as f:
        data = json.load(f)
    return hashlib.sha256(json.dumps(data, sort_keys=True).encode()).hexdigest()[:16]


def header_sha(header_path):
    with open(header_path) as f:
        for line in f:
            if "BPE_SOURCE_SHA" in line:
                return line.split("BPE_SOURCE_SHA")[1].strip(" */\n")
    return None


def main():
    tokenizer = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        ROOT, "data", "tokenizer.json")
    header = sys.argv[2] if len(sys.argv) > 2 else os.path.join(
        ROOT, "runtime", "bpe.h")

    if not os.path.exists(tokenizer):
        print(f"skip: {tokenizer} not present")
        return 0
    want = source_sha(tokenizer)
    got = header_sha(header)
    if got != want:
        print(f"STALE: {header} was built from {got}, but {tokenizer} is {want}")
        print("Run: uv run --with sympy python export_tokenizer.py "
              f"--tokenizer {tokenizer}")
        return 1
    print(f"ok: bpe.h matches {os.path.basename(tokenizer)} (sha {want})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
