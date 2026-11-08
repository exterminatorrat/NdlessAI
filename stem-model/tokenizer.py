import json
from collections import Counter

from relations import RELATIONS

PAD, BOS, EOS, CALL, SEP = 256, 257, 258, 259, 260
REL_BASE = 261
FIRST_MERGE = REL_BASE + len(RELATIONS)

# Id layout of a VOCAB_SIZE vocabulary: 0-255 byte fallbacks, 256-260 the
# specials, 261 and up one reserved id per sorted relation name, then learned
# merges to the end. Relations and merges share the ids past REL_BASE, so the
# relation count and the merge count trade off one for one: at 1024 the last
# relation id is 1023, which caps the relation set at 1024 - 261 = 763.
VOCAB_SIZE = 1024

SPECIALS = {"<pad>": PAD, "<bos>": BOS, "<eos>": EOS, "CALL": CALL, "<sep>": SEP}
SPECIALS.update({name: REL_BASE + i for i, name in enumerate(sorted(RELATIONS))})
SPECIAL_PIECES = sorted(SPECIALS, key=len, reverse=True)


def split_specials(text):
    pieces = []
    i = 0
    while i < len(text):
        for piece in SPECIAL_PIECES:
            if text.startswith(piece, i):
                pieces.append((True, piece))
                i += len(piece)
                break
        else:
            j = i
            while j < len(text) and not any(text.startswith(p, j) for p in SPECIAL_PIECES):
                j += 1
            pieces.append((False, text[i:j]))
            i = j
    return pieces


class Tokenizer:
    def __init__(self, merges):
        self.merges = [tuple(m) for m in merges]
        self.ranks = {pair: FIRST_MERGE + i for i, pair in enumerate(self.merges)}
        self.vocab = {i: bytes([i]) for i in range(256)}
        for pair, idx in self.ranks.items():
            self.vocab[idx] = self.vocab[pair[0]] + self.vocab[pair[1]]
        for piece, idx in SPECIALS.items():
            self.vocab[idx] = piece.encode()

    @property
    def size(self):
        return FIRST_MERGE + len(self.merges)

    def _encode_bytes(self, data):
        ids = list(data)
        while len(ids) > 1:
            best = None
            for i in range(len(ids) - 1):
                rank = self.ranks.get((ids[i], ids[i + 1]))
                if rank is not None and (best is None or rank < best[0]):
                    best = (rank, i)
            if best is None:
                break
            _, i = best
            ids[i:i + 2] = [self.ranks[(ids[i], ids[i + 1])]]
        return ids

    def encode(self, text):
        out = []
        for is_special, piece in split_specials(text):
            out.extend([SPECIALS[piece]] if is_special else self._encode_bytes(piece.encode()))
        return out

    def decode(self, ids):
        return b"".join(self.vocab[i] for i in ids).decode("utf-8", "replace")

    def to_json(self):
        return {"merges": [list(m) for m in self.merges]}

    @classmethod
    def from_json(cls, data):
        return cls(data["merges"])


def count_pairs(ids_list):
    counts = Counter()
    for ids in ids_list:
        for i in range(len(ids) - 1):
            counts[(ids[i], ids[i + 1])] += 1
    return counts


def train(texts, vocab_size=VOCAB_SIZE):
    if vocab_size < FIRST_MERGE:
        raise ValueError(
            f"vocab_size {vocab_size} below the {FIRST_MERGE} reserved ids; "
            f"minimum is {FIRST_MERGE + 1}")

    encoded = []
    for text in texts:
        for is_special, piece in split_specials(text):
            encoded.append([SPECIALS[piece]] if is_special else list(piece.encode()))

    merges = []
    while FIRST_MERGE + len(merges) < vocab_size:
        counts = count_pairs(encoded)
        counts = {p: c for p, c in counts.items() if c > 1}
        if not counts:
            break
        pair = max(sorted(counts), key=lambda p: counts[p])
        new_id = FIRST_MERGE + len(merges)
        merges.append(pair)
        merged = []
        for ids in encoded:
            out = []
            i = 0
            while i < len(ids):
                if i + 1 < len(ids) and (ids[i], ids[i + 1]) == pair:
                    out.append(new_id)
                    i += 2
                else:
                    out.append(ids[i])
                    i += 1
            merged.append(out)
        encoded = merged
    return Tokenizer(merges)


def main():
    import argparse

    ap = argparse.ArgumentParser()
    ap.add_argument("--traces", default="traces.jsonl")
    ap.add_argument("--vocab", type=int, default=VOCAB_SIZE)
    ap.add_argument("--out", default="tokenizer.json")
    args = ap.parse_args()

    rows = [json.loads(line) for line in open(args.traces)]
    texts = [f"{r['question']} <sep> {r['call']}" for r in rows]
    tok = train(texts, args.vocab)

    encoded = [tok.encode(t) for t in texts]
    lengths = [len(e) for e in encoded]
    calls = [len(tok.encode(r["call"])) for r in rows]
    round_trip = sum(tok.decode(e) == t for e, t in zip(encoded, texts))

    print(f"vocab {tok.size} ({len(tok.merges)} merges)")
    print(f"mean tokens/question {sum(lengths) / len(lengths):.1f}, max {max(lengths)}")
    print(f"mean tokens/call {sum(calls) / len(calls):.1f}, max {max(calls)}")
    print(f"round-trip exact: {round_trip}/{len(texts)}")

    with open(args.out, "w") as f:
        json.dump(tok.to_json(), f)
    print(f"wrote {args.out}")

    sample = rows[0]
    ids = tok.encode(f"{sample['question']} <sep> {sample['call']}")
    print(f"\nsample: {tok.decode(ids)}")
    print(f"ids: {ids}")


if __name__ == "__main__":
    main()
