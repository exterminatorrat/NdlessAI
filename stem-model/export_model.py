import argparse
import os
import struct

import numpy as np
import torch

from export_int8 import SCALE_BITS, quantize
from train import Config, Model

MAGIC = b"NSM1"
QT_GROUP = 32

STACKS = [
    ("wq", "attn.wq.weight"),
    ("wk", "attn.wk.weight"),
    ("wv", "attn.wv.weight"),
    ("wo", "attn.wo.weight"),
    ("w1", "w1.weight"),
    ("w3", "w3.weight"),
    ("w2", "w2.weight"),
]


def dequant(q, s):
    w = np.zeros(q.shape, dtype=np.float64)
    for c0 in range(0, q.shape[1], QT_GROUP):
        scale = s[:, c0 // QT_GROUP].astype(np.float64) / (1 << SCALE_BITS)
        w[:, c0:c0 + QT_GROUP] = q[:, c0:c0 + QT_GROUP] * scale[:, None]
    return w


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ckpt", default="model.pt")
    ap.add_argument("--out", default="weights.bin")
    ap.add_argument("--ref-ids", default="259,32,261,32,258")
    ap.add_argument("--ref-out", default="")
    args = ap.parse_args()

    ck = torch.load(args.ckpt, map_location="cpu", weights_only=False)
    cfg = Config(**ck["config"])
    state = ck["model"]
    ids = [int(x) for x in args.ref_ids.split(",") if x.strip()]
    if ids and max(ids) >= cfg.vocab:
        raise SystemExit(f"ref id {max(ids)} >= vocab {cfg.vocab}")

    def stack(suffix):
        return np.concatenate(
            [state[f"blocks.{l}.{suffix}"].numpy().astype(np.float64)
             for l in range(cfg.layers)], axis=0)

    tensors = [("tok_emb", state["tok_emb.weight"].numpy().astype(np.float64))]
    for name, suffix in STACKS:
        tensors.append((name, stack(suffix)))

    quant, ref_state = {}, {k: v.clone() for k, v in state.items()}

    for name, w in tensors:
        q, s = quantize(w)
        quant[name] = (q.astype(np.int8), s.astype("<i4"))
        dq = dequant(q, s)
        print(f"  {name:8s} {w.shape[0]:5d}x{w.shape[1]:4d} "
              f"max|err| {np.abs(dq - w).max():.2e}")

        rows = dq.shape[0]
        if name == "tok_emb":
            ref_state["tok_emb.weight"] = torch.from_numpy(dq).float()
            ref_state["head.weight"] = torch.from_numpy(dq).float()
        else:
            per = rows // cfg.layers
            for l in range(cfg.layers):
                key = f"blocks.{l}.{dict(STACKS)[name] if name in dict(STACKS) else name}"
                ref_state[key] = torch.from_numpy(dq[l * per:(l + 1) * per]).float()

    with open(args.out, "wb") as f:
        f.write(MAGIC)
        f.write(struct.pack("<5i", cfg.dim, cfg.layers, cfg.heads, cfg.hidden, cfg.vocab))
        for name in ["tok_emb"] + [n for n, _ in STACKS]:
            f.write(quant[name][0].tobytes())
            f.write(quant[name][1].tobytes())
        for l in range(cfg.layers):
            f.write(state[f"blocks.{l}.n1.weight"].numpy().astype("<f4").tobytes())
        for l in range(cfg.layers):
            f.write(state[f"blocks.{l}.n2.weight"].numpy().astype("<f4").tobytes())
        f.write(state["norm.weight"].numpy().astype("<f4").tobytes())

    print(f"wrote {args.out} ({os.path.getsize(args.out) / 1e6:.2f} MB)")

    if args.ref_out:
        model = Model(cfg)
        model.load_state_dict(ref_state)
        model.eval()
        with torch.no_grad():
            logits, _ = model(torch.tensor([ids]))
        np.save(args.ref_out, logits[0].numpy())
        print(f"reference logits {tuple(logits.shape)} -> {args.ref_out}")


if __name__ == "__main__":
    main()
