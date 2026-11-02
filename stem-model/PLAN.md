# Plan: a small STEM model on a TI-Nspire CX II

Target: on the calculator, in a few seconds, turn a typed school problem into
an exact answer. Bounded by what the hardware actually measures, not what it
was advertised to do.

## 1. Measured facts

From `docs/HARDWARE.md` in the nspire-tlm project, measured on a CX II CAS:

| Quantity | Measured |
|---|---|
| Largest single `malloc` | 21.56 MiB (22.6 MB) |
| Total heap in 64 KB chunks | 27.12 MiB (28.4 MB) |
| LPDDR sequential read | 70.5 MB/s |
| `memcpy` | 50.9 MB/s |
| I-cache / D-cache | 16 KB / 8 KB, 32-byte lines, 4-way |
| SPI NAND sequential | 1.46 MB/s |
| Random 4 KB flash latency | 58 ms |

Two constraints follow immediately:

- **One buffer cannot hold the whole model.** Weights must be allocated per
  layer, so the runtime allocates `L` buffers, not one.
- **Weights are re-read every token.** 5.46M int8 weights at 70.5 MB/s sets a
  77 ms/token floor regardless of compute.

The CPU clock is **disputed**, and this matters more than anything else here:
TI documentation and community CoreMark runs say 396 MHz, while the measured
figure in the same project is 198 MHz on a first run and **144 MHz on
subsequent runs**. `int8bench` exists to settle it on the unit in hand. Every
number below is quoted at both 144 and 198 MHz.

## 2. Cycle model

A batch-1 decode token costs about one MAC per parameter, plus attention, which
is small at the short contexts this model uses. The disassembly of the
benchmark kernel is 5 instructions per MAC (`ldrsb`, `ldrsb`, `cmp`, `smlabb`,
`bne`) on a single-issue ARM926EJ-S, so:

    cycles/token  ~= 5 * params

Cross-check: nspire-tlm's own derived table gives 15M parameters at 2.33 tok/s
at 144 MHz, which implies 4.1 cycles per parameter. Our 5.0 is conservative by
about 20%, which is the right direction for planning.

## 3. Model size

Parameter count for a llama2-style decoder with tied embeddings, verified
against a published model:

    N = V*d + L*(4d^2 + 3*d*hidden + 2d) + d

Check: d=352, L=6, V=4096, hidden=1024 gives 10,908,128, which is exactly
nspire-tlm's published count.

| d | L | vocab | hidden | params | int8 size | ms/tok @144 | tok/s | ms/tok @198 | tok/s |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 128 | 6 | 512 | 512 | 1,640,064 | 1.64 MB | 57 | 17.6 | 41 | 24.1 |
| 256 | 6 | 512 | 768 | 5,246,208 | 5.25 MB | 182 | 5.5 | 132 | 7.6 |
| 352 | 6 | 512 | 1024 | 9,646,560 | 9.65 MB | 335 | 3.0 | 244 | 4.1 |

**Decision: d=256 (5.25M).** It leaves 21 MB of the 27 MB heap for the KV cache
and scratch, clears the 77 ms/token bandwidth floor with margin, and is the
smallest width that satisfies the int8 group constraint below.

`d` must be a multiple of the int8 group size, because a per-group scale cannot
span a partial group. Group 32 forces d to be a multiple of 32, which rules out
176 (5.5 groups) and 264 (8.25 groups) even though those widths look natural.
With group 32 the whole weight matrix has 12.5% scale overhead; group 8 would
divide any width but cost 50%. d=256 with group 32 keeps both small.

The parameter counts in the table are not estimates: `train.py` builds each
configuration and reports them, matching the formula exactly. The counts were
re-verified after adding rotary embeddings, which add no parameters.

Output length is much shorter than assumed. With relation names as single
tokens, a measured call is **7.4 tokens** (max 11), not 24. At 5.3 tok/s that
is **1.4 s** per answer at 144 MHz, or 1.0 s at 198 MHz.

Evidence on size, from nspire-tlm's width ladder (6 layers, widths 176/264/352):
`d176` (~3M) was only **2.5 points worse on grounded computation** but **69.3
points worse on `explain`**. That is the single most useful result for this
design: shrinking hurts open-ended explanation, not call selection. Put
knowledge in code, keep the model on grounded calls, and small works.

## 4. One model or three

**One model, three relation tables.** The language job is identical across
subjects (find quantities, pick an operation, fill slots), so three models
triple the data and training work for no extra capability. Subject separation
belongs in the tables and solvers, which cost kilobytes. Revisit only if
chemistry nomenclature alone proves unlearnable at this width.

## 5. Data

The model emits `CALL <relation> <args>` and never a number; the runtime
executes it. `relations.py` is the contract.

- **Volume:** ~1M traces, ~52 tokens each (44 question + 7.4 call), roughly
  52M tokens. nspire-tlm
  shipped on an 8,000-step run and found 17,750 steps no better, so ~100M
  tokens of training is the working budget, i.e. ~2 epochs.
- **Distractors:** 50% of rows carry one plausible same-unit extra quantity.
  This is the highest-value data lever available: nspire-tlm reports 116/120
  dropping to 61/120 when one irrelevant value precedes the relevant ones.
- **Splits:** train / val / dev / fresh, disjoint by generator seed and by
  template, so an apparent gain cannot come from memorised wording.
- **Execution check:** every row is executed and compared to its solver. This
  already runs; 5,500 rows across 20 seeds, 0 mismatches.

## 6. Tokenizer

Domain BPE at 512 tokens over question text plus call vocabulary, with byte
fallback, and the call delimiters as reserved single tokens.

Position information comes from rotary embeddings (theta 10000), applied to q
and k per head, matching llama2. This matters for this task specifically: with
causal masking alone the model is close to permutation-invariant, and the CALL
arguments are distinguished only by their order. The trainer and the C engine
implement the same rotation, and the parity test covers it.

Implemented in `tokenizer.py` and measured on 12,000 traces: 240 merges,
**43.9 tokens per question**, **7.4 tokens per call**, and 12,000/12,000 exact
round-trips. `CALL` and each relation name are single tokens, which is what
makes the call so short and what makes constrained decoding a small trie.

Rationale: at V=4096 and d=264 the embedding table is 1.08M parameters, 20% of
the model spent on vocabulary. At V=512 it is 2.5%. That is the cheapest
capacity win available.

## 7. Training

Copy the recipe that is known to work at this scale rather than inventing one:
AdamW, lr 3e-4, 3% warmup, cosine decay to 10%, weight decay 0.1, batch 24,
seq 512, 8,000 steps.

**Mask the loss to the call tokens.** This is not a detail. Training on the
whole sequence spends almost all of the gradient reproducing the question, and
the model learns to emit `CALL` and then repeat it. Measured on CPU at d=176:
full-sequence loss reached 4.71 in 347 steps and produced
`CALL CALL CALL CALL`; call-only loss reached 3.10 in 431 steps and produced
the correct relation name on all three held-out samples. Deployment only needs
the call, so the question tokens should never contribute loss.

Hardware: an RTX 4070 with 12 GB is comfortably sufficient. Weights 22 MB,
AdamW moments 44 MB, activations small; the whole run fits in well under 2 GB,
so batch size can be raised for speed.

Time, from `6 * N * D` FLOPs: 8,000 steps at 5.46M parameters is 3.2e15 FLOPs,
about **6 minutes** at 9 TFLOP/s effective. Even allowing for a 4x
inefficiency this is under half an hour. nspire-tlm spent 156 minutes on the
same shape of run, so treat this as an optimistic bound until measured.

Measured on 2 CPU cores, d=176, batch 32, seq 128: about **0.9 s per step**, so
8,000 steps would be roughly 2 hours on CPU. A 4070 should be one to two orders
of magnitude faster; this is the number to beat.

## 8. Quantization and export

Per-channel int8 with group-32 blocks and **Q24** fixed-point scales.

Q16 was measured to be the dominant error term: for weights with standard
deviation 0.02 the scale is only about 46 in Q16, i.e. 6 significant bits, which
tripled the weight error (3.4e-4 with exact scales versus 1.1e-3 in Q16). Q24
brings it back to 3.4e-4, and an int32 scale holds 2^24 * 0.1 comfortably.

One consequence for the device port: the accumulator must be int64 (or float)
because `acc * scale` reaches 2^31 in Q24. The host engine uses float, which is
exact enough here but slow on a chip without an FPU; the deploy kernel should
keep int32 accumulators, dequantize with a shift, and stay in fixed point.

Group 32 instead of nspire-tlm's 88: 32 divides the power-of-two dimensions and
keeps loads aligned. Weights go out as packed `uint32_t` in the layout
`int8bench` reads.

## 9. On-device runtime

- Per-layer weight buffers, each under 21.5 MB, total under 27 MB.
- int8 dot product with `SMLABB` contraction; do not hand-roll packing, the
  benchmark shows it does not help.
- Fixed-point RMSNorm; `exp` for softmax from a table, since there is no FPU.
- KV cache at int8: 1.62 MB at d=264, seq 512.
- Grammar-constrained decoding from a trie of valid next tokens, so an invalid
  call cannot be produced and no retry is needed.
- One repair attempt on a failed call, then a declared failure rather than
  prose arithmetic.

## 10. Evaluation

Report exact-match accuracy and wrong-answer rate separately, on held-out
generators, with and without distractors. Loss is not the metric; a model that
declines is better than one that guesses.

Acceptance gates:

| Gate | Threshold |
|---|---|
| Relation-supplied, no distractor | >= 95% correct |
| Relation-supplied, with distractor | >= 85% correct |
| End-to-end fresh problems | >= 70% correct, <= 5% wrong |
| Rule-based baseline | measured on the same sets; the neural path must beat it where rules cannot apply |

nspire-tlm's own finding sets the bar: their rule-based path was more often
correct than the model on every numeric set, so any neural version has to
justify itself against a deterministic baseline, not against nothing.

## 11. Milestones

| # | Deliverable | Verified by |
|---|---|---|
| M0 | `int8bench` measures real tok/s and clock on the unit | numbers recorded in the PR |
| M1 | Trace generator + distractors | every row executed, 0 mismatches |
| M2 | Training run at d=264 | val loss falls, then plateaus |
| M3 | int8 export | dot-product error < 2% |
| M4 | C runtime on device | matches host outputs on 100 held-out calls |
| M5 | Evaluation on fresh sets | gates above |

## 12. Risks

- **Clock uncertainty (factor 2.5).** Everything above moves with it. M0
  settles it; no model size should be fixed before then.
- **Distractor brittleness.** Largest known accuracy risk, and a data problem
  rather than a size problem.
- **Soft-float drift.** No FPU, so numerics are fixed point. Mitigated by
  executing arithmetic in exact code, but RMSNorm and softmax still need
  care.
- **Overfitting at 100M tokens.** nspire-tlm saw no gain past 8,000 steps and
  used more unique data rather than more steps. Expect the same.
- **Tokenizer on math text.** Digits, `^`, and implicit multiplication
  tokenize badly under generic BPE; this is why the vocabulary is domain
  specific and checked before training, not after.
