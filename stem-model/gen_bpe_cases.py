"""Regenerate runtime/fixtures/bpe_cases.txt from the current tokenizer.

The fixture records, for a set of texts, the exact ids the Python tokenizer
produces, and test_bpe replays it against the C encoder. A vocabulary change
makes every recorded id stale, and the same failure mode as a stale bpe.h
follows: the fixture still runs, still passes on the old vocabulary, and stops
meaning anything on the new one. Regenerate it here rather than by hand.

    python3 gen_bpe_cases.py            # rewrite the IDS lines
    python3 gen_bpe_cases.py --check    # exit 1 if the checked-in fixture is stale
"""
import argparse
import json
import os
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ROOT)

from tokenizer import Tokenizer  # noqa: E402

DEFAULT_FIXTURE = os.path.join(ROOT, "runtime", "fixtures", "bpe_cases.txt")
DEFAULT_TOKENIZER = os.path.join(ROOT, "data", "tokenizer.json")


def ids_line(ids):
    return "IDS\t" + " ".join(str(i) for i in ids)


def unescape(s):
    """The fixture's TEXT escaping, mirroring test_bpe.c's unescape."""
    out = []
    i = 0
    while i < len(s):
        if s[i] == "\\" and i + 1 < len(s):
            c = {"n": "\n", "t": "\t", "\\": "\\"}.get(s[i + 1])
            if c is None:
                raise SystemExit("unknown escape in fixture: \\%s" % s[i + 1])
            out.append(c)
            i += 2
        else:
            out.append(s[i])
            i += 1
    return "".join(out)


def rewrite(lines, tok):
    """Yield the fixture with every IDS line recomputed. Returns (lines, stale)."""
    out = []
    stale = 0
    text = None
    for line in lines:
        if line.startswith("TEXT\t"):
            text = unescape(line[len("TEXT\t"):].rstrip("\n"))
            out.append(line)
        elif line.startswith("IDS\t"):
            if text is None:
                raise SystemExit("IDS line with no preceding TEXT line")
            want = ids_line(tok.encode(text))
            if want.rstrip("\n") != line.rstrip("\n"):
                stale += 1
            out.append(want + "\n")
        else:
            out.append(line)
            if line.startswith("#"):
                text = None
    return out, stale


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fixture", default=DEFAULT_FIXTURE)
    ap.add_argument("--tokenizer", default=DEFAULT_TOKENIZER)
    ap.add_argument("--check", action="store_true",
                    help="exit 1 when the fixture does not match the tokenizer")
    args = ap.parse_args()

    if not os.path.exists(args.tokenizer):
        print("skip: %s does not exist" % args.tokenizer)
        return 77
    with open(args.tokenizer) as f:
        tok = Tokenizer.from_json(json.load(f))
    with open(args.fixture) as f:
        lines = f.readlines()

    updated, stale = rewrite(lines, tok)
    if args.check:
        if stale:
            print("STALE: %s has %d case(s) that do not match %s"
                  % (args.fixture, stale, args.tokenizer))
            return 1
        print("ok: %s matches %s" % (args.fixture, args.tokenizer))
        return 0

    with open(args.fixture, "w") as f:
        f.writelines(updated)
    print("rewrote %s: %d case(s) updated" % (args.fixture, stale))
    return 0


if __name__ == "__main__":
    sys.exit(main())
