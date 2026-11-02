import argparse
import json
import math
import time

import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F

from tokenizer import Tokenizer

THETA = 10000.0


def rope(x, theta=THETA):
    b, h, t, hd = x.shape
    half = hd // 2
    freq = 1.0 / (theta ** (torch.arange(0, half, dtype=torch.float32) / half))
    ang = torch.outer(torch.arange(t, dtype=torch.float32), freq)
    cos, sin = ang.cos()[None, None], ang.sin()[None, None]
    x1, x2 = x[..., :half], x[..., half:]
    return torch.cat([x1 * cos - x2 * sin, x1 * sin + x2 * cos], dim=-1)


class Config:
    def __init__(self, dim, layers, heads, hidden, vocab, seq):
        self.dim = dim
        self.layers = layers
        self.heads = heads
        self.hidden = hidden
        self.vocab = vocab
        self.seq = seq


class RMSNorm(nn.Module):
    def __init__(self, dim):
        super().__init__()
        self.eps = 1e-5
        self.weight = nn.Parameter(torch.ones(dim))

    def forward(self, x):
        return self.weight * x * torch.rsqrt(x.pow(2).mean(-1, keepdim=True) + self.eps)


class Attention(nn.Module):
    def __init__(self, cfg):
        super().__init__()
        self.heads = cfg.heads
        self.head_dim = cfg.dim // cfg.heads
        self.wq = nn.Linear(cfg.dim, cfg.dim, bias=False)
        self.wk = nn.Linear(cfg.dim, cfg.dim, bias=False)
        self.wv = nn.Linear(cfg.dim, cfg.dim, bias=False)
        self.wo = nn.Linear(cfg.dim, cfg.dim, bias=False)

    def forward(self, x):
        b, t, _ = x.shape
        q, k, v = self.wq(x), self.wk(x), self.wv(x)
        q = q.view(b, t, self.heads, self.head_dim).transpose(1, 2)
        k = k.view(b, t, self.heads, self.head_dim).transpose(1, 2)
        v = v.view(b, t, self.heads, self.head_dim).transpose(1, 2)
        q, k = rope(q), rope(k)
        out = F.scaled_dot_product_attention(q, k, v, is_causal=True)
        return self.wo(out.transpose(1, 2).reshape(b, t, -1))


class Block(nn.Module):
    def __init__(self, cfg):
        super().__init__()
        self.n1 = RMSNorm(cfg.dim)
        self.attn = Attention(cfg)
        self.n2 = RMSNorm(cfg.dim)
        self.w1 = nn.Linear(cfg.dim, cfg.hidden, bias=False)
        self.w2 = nn.Linear(cfg.hidden, cfg.dim, bias=False)
        self.w3 = nn.Linear(cfg.dim, cfg.hidden, bias=False)

    def forward(self, x):
        x = x + self.attn(self.n1(x))
        h = self.w1(self.n2(x))
        return x + self.w2(F.silu(h) * self.w3(self.n2(x)))


class Model(nn.Module):
    def __init__(self, cfg):
        super().__init__()
        self.cfg = cfg
        self.tok_emb = nn.Embedding(cfg.vocab, cfg.dim)
        self.blocks = nn.ModuleList(Block(cfg) for _ in range(cfg.layers))
        self.norm = RMSNorm(cfg.dim)
        self.head = nn.Linear(cfg.dim, cfg.vocab, bias=False)
        self.head.weight = self.tok_emb.weight
        self.apply(self._init)

    @staticmethod
    def _init(module):
        if isinstance(module, nn.Linear):
            nn.init.normal_(module.weight, mean=0.0, std=0.02)

    def forward(self, idx, targets=None):
        x = self.tok_emb(idx)
        for block in self.blocks:
            x = block(x)
        logits = self.head(self.norm(x))
        if targets is None:
            return logits, None
        return logits, F.cross_entropy(logits.view(-1, logits.size(-1)), targets.view(-1))

    def n_params(self):
        return sum(p.numel() for p in self.parameters())


@torch.no_grad()
def generate(model, tok, prompt, max_new=16):
    model.eval()
    ids = tok.encode(prompt)
    for _ in range(max_new):
        idx = torch.tensor([ids[-model.cfg.seq:]])
        logits, _ = model(idx)
        nxt = int(logits[0, -1].argmax())
        ids.append(nxt)
        if nxt == 258:
            break
    return tok.decode(ids)


def pack(rows, tok, seq):
    cap = seq + 1
    windows, skipped = [], 0
    cur_ids, cur_mask = [], []

    for r in rows:
        prompt = tok.encode(f"{r['question']} <sep>")
        call = tok.encode(r["call"])
        ids = prompt + call + [258]
        mask = [False] * len(prompt) + [True] * (len(call) + 1)
        if len(ids) > cap:
            skipped += 1
            continue
        if len(cur_ids) + len(ids) > cap:
            pad = cap - len(cur_ids)
            windows.append((cur_ids + [256] * pad, cur_mask + [False] * pad))
            cur_ids, cur_mask = [], []
        cur_ids.extend(ids)
        cur_mask.extend(mask)

    if cur_ids:
        pad = cap - len(cur_ids)
        windows.append((cur_ids + [256] * pad, cur_mask + [False] * pad))

    if skipped:
        print(f"warning: skipped {skipped} rows longer than {cap} tokens")
    if not windows:
        raise SystemExit(
            f"no training windows: need rows shorter than {cap} tokens and at least one row")

    flat = np.asarray([w[0] for w in windows], dtype=np.int64)
    mask = np.asarray([w[1] for w in windows], dtype=bool)
    return flat[:, :seq], flat[:, 1:], mask[:, 1:]


def batch(x, y, m, size, rng):
    idx = rng.integers(0, len(x), size=size)
    return torch.from_numpy(x[idx]), torch.from_numpy(y[idx]), torch.from_numpy(m[idx])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--traces", default="traces.jsonl")
    ap.add_argument("--tokenizer", default="tokenizer.json")
    ap.add_argument("--dim", type=int, default=256)
    ap.add_argument("--layers", type=int, default=6)
    ap.add_argument("--heads", type=int, default=8)
    ap.add_argument("--hidden", type=int, default=768)
    ap.add_argument("--seq", type=int, default=512)
    ap.add_argument("--batch", type=int, default=24)
    ap.add_argument("--steps", type=int, default=8000)
    ap.add_argument("--lr", type=float, default=3e-4)
    ap.add_argument("--warmup", type=float, default=0.03)
    ap.add_argument("--eval-every", type=int, default=500)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--out", default="model.pt")
    ap.add_argument("--max-minutes", type=float, default=0.0)
    ap.add_argument("--samples", type=int, default=3)
    args = ap.parse_args()

    torch.manual_seed(args.seed)
    rng = np.random.default_rng(args.seed)

    if args.dim % args.heads:
        raise SystemExit(f"--dim {args.dim} is not divisible by --heads {args.heads}")
    if args.dim % 32:
        raise SystemExit(f"--dim {args.dim} must be a multiple of 32 for int8 group quantization")

    rows = [json.loads(line) for line in open(args.traces)]
    tok = Tokenizer.from_json(json.load(open(args.tokenizer)))
    split = int(len(rows) * 0.98)

    def text_of(r):
        return f"{r['question']} <sep> {r['call']}"

    train_x, train_y, train_m = pack(rows[:split], tok, args.seq)
    val_x, val_y, val_m = pack(rows[split:], tok, args.seq)
    print(f"train windows {len(train_x)}, val windows {len(val_x)}, vocab {tok.size}")

    def loss_of(logits, targets, mask):
        targets = targets.masked_fill(~mask, -100)
        return F.cross_entropy(logits.view(-1, logits.size(-1)), targets.view(-1),
                               ignore_index=-100)

    cfg = Config(args.dim, args.layers, args.heads, args.hidden, tok.size, args.seq)
    model = Model(cfg)
    print(f"parameters {model.n_params():,}")

    decay = [p for p in model.parameters() if p.dim() >= 2]
    no_decay = [p for p in model.parameters() if p.dim() < 2]
    opt = torch.optim.AdamW(
        [{"params": decay, "weight_decay": 0.1},
         {"params": no_decay, "weight_decay": 0.0}], lr=args.lr)

    warm = max(1, int(args.steps * args.warmup))

    def lr_at(step):
        if step < warm:
            return args.lr * (step + 1) / warm
        progress = (step - warm) / max(1, args.steps - warm)
        return args.lr * (0.1 + 0.9 * 0.5 * (1 + math.cos(math.pi * progress)))

    start = time.time()
    model.train()
    for step in range(args.steps):
        for group in opt.param_groups:
            group["lr"] = lr_at(step)
        bx, by, bm = batch(train_x, train_y, train_m, args.batch, rng)
        logits, _ = model(bx)
        loss = loss_of(logits, by, bm)
        opt.zero_grad(set_to_none=True)
        loss.backward()
        torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
        opt.step()

        if (step + 1) % args.eval_every == 0:
            model.eval()
            with torch.no_grad():
                vx, vy, vm = batch(val_x, val_y, val_m, min(16, len(val_x)), rng)
                vlogits, _ = model(vx)
                vloss = loss_of(vlogits, vy, vm)
            print(f"step {step + 1:5d} loss {loss.item():.4f} val {vloss.item():.4f} "
                  f"lr {lr_at(step):.2e} {time.time() - start:.0f}s", flush=True)
            model.train()
        if args.max_minutes and time.time() - start > args.max_minutes * 60:
            print(f"stopping at {step + 1} steps (time limit)")
            break

    torch.save({"model": model.state_dict(),
                "config": vars(cfg)}, args.out)
    print(f"saved {args.out}")

    for row in rows[split:split + args.samples]:
        prompt = f"{row['question']} <sep>"
        out = generate(model, tok, prompt, max_new=16)
        got = out[len(prompt):].strip()
        ok = got.split("<eos>")[0].strip() == row["call"]
        print(f"\n{'OK  ' if ok else 'miss'} expected: {row['call']}")
        print(f"     got:      {got}")


if __name__ == "__main__":
    main()
