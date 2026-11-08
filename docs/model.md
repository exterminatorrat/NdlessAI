# Model

NdlessAI's model is a small decoder that translates one English word problem into one `CALL` line. It is not a general calculator, a chat model, or a derivation generator. Its job is language understanding; the mathematics belongs to `solvers.c`.

## Intended use

- Answer generated or hand-written word problems that match one of the 66 supported relations.
- Run offline on a TI-Nspire CX II, with no account, network, or external service.
- Produce an exact answer, or refuse, rather than produce a plausible but unverified one.

Out of scope: free-form tutoring, multi-step problems, symbolic manipulation outside the relation set, diagrams, units outside the templates, and any problem that needs a number not present in the question.

## Data

Questions are synthetic by design, because every training row can then be executed and checked.

| Property | Value |
| --- | --- |
| Relations | 66 across precalculus, calculus, trig, logs, algebra, physics, chemistry, and units |
| Phrasings | At least 8 per relation; every fifth index is held out |
| Distractors | One same-unit extra quantity in about 50% of rows |
| Default dataset | 200,000 train rows, 4,000 validation rows, 2,000 held-out-wording rows |
| Recorded dataset | 200,000 rows, 47 MB, generated in about 11 minutes; 100,444 rows carry a distractor |

Each row contains the subject, question, call, expected answer, and optional distractor. `make_dataset.py` renders the question through a phrasing bank, generates parameters with the relation's own generator, executes the resulting call, and checks that the answer matches the relation's solver. Any mismatch aborts generation.

The three splits are disjoint by seed and by phrasing:

- **Train** uses the training phrasing indices.
- **Validation** uses different parameter seeds and the training phrasings.
- **Held-out** uses the reserved phrasings, which never appear in training.

`surface.py` reserves every fifth phrasing, spread through the list, rather than the last one, so the held-out set is not systematically the strangest wording.

## Tokenizer

The tokenizer is byte-level BPE with a 1024-token vocabulary:

| IDs | Contents |
| --- | --- |
| 0-255 | Byte fallback |
| 256-260 | `<pad>`, `<bos>`, `<eos>`, `CALL`, `<sep>` |
| 261-326 | One reserved id per relation name, 66 today |
| 327-1023 | Learned merges |

Relation names and merges share every id from 261 up, so the vocabulary size is also the ceiling on the relation set. With no merges left, 512 reserved `512 - 261 = 251` relation ids and 1024 reserves `1024 - 261 = 763`. At today's 66 relations `FIRST_MERGE` is 327, which leaves `512 - 327 = 185` merges at 512 and `1024 - 327 = 697` at 1024. The merges are what buy latency: every merge shortens the prompt, and prefill is most of the on-device answer time.

The retrained checkpoint and the regenerated `bpe.h` that go with 1024 are a separate change, so the token counts below are still the 512-token ones and nothing here has been measured at 1024.

The checkpoint and the C header must come from the same `tokenizer.json`. `export_tokenizer.py` embeds a SHA-256 fingerprint of the source tokenizer in `bpe.h`, and `check_tokenizer_sync.py` verifies it. A previous run scored 0/40 because a checkpoint and header came from different dataset runs; matching them fixed it.

Measured on 12,000 traces with the 512-token vocabulary: 43.9 tokens per question, 7.4 tokens per call, and 12,000/12,000 exact tokenizer round-trips. More merges are expected to cut the per-question count; that is the reason for the wider vocabulary and it has not been measured.

## Architecture and training

The model is a Llama-2-style decoder:

| Parameter | Value |
| --- | --- |
| Layers | 6 |
| Width | 256 |
| Heads | 8 |
| Feed-forward width | 768 |
| Vocabulary | 1024 |
| Sequence length | 512 by default; the `make train` target uses 128-token windows |
| Parameters | 5,377,280 |
| Positional encoding | RoPE, theta 10000 |
| Normalization | RMSNorm |
| Activation | SwiGLU |
| Embeddings | Tied input and output |

`train.py` defaults: AdamW with learning rate `3e-4`, 3% warmup, cosine decay to 10%, weight decay `0.1` for matrices, gradient clipping at `1.0`, sequence length 512, batch 24, and 8,000 steps. The `make train` target uses the same optimizer and model but 128-token windows, batch 32, and 6,000 steps. The split is the last 2% of rows. Windows are packed without crossing a row boundary, so a call is never trained against a truncated prompt.

The loss is masked to the call tokens. Training on the whole sequence taught the model to recite the question and then repeat `CALL`; call-only loss produced correct relation names on held-out samples.

The recorded run trained at `d=256` with the 512-token vocabulary and reached validation loss `1.068` at step 100 and `0.066` at step 500. It took 834 seconds on two CPU cores, and all three sampled validation problems matched exactly. These are early-run numbers, not a full 8,000-step result.

## Quantization and export

`export_model.py` writes a self-describing `NSM1` file:

```text
4-byte magic "NSM1"
dim, layers, heads, hidden, vocab
tok_emb: int8 weights + int32 group scales
wq, wk, wv, wo, w1, w3, w2: stacked over layers, int8 + int32 scales
n1, n2, norm: float parameters
```

Quantization is per output channel with groups of 32 columns. Scales are stored as Q24 fixed-point integers so the device never needs to interpret a float scale. The recorded export is 5.91 MB, and the maximum per-group weight error is `4.2e-4`. Q16 was measured and rejected because its reconstruction error was too large.

`runtime/test_fixed.c` compares the fixed-point forward pass with the float reference on six inputs, including real token ids and synthetic sequences. The recorded run matched logits to `7.4e-07` with identical argmax.

## Evaluation

Two evaluators drive the C code on held-out validation rows:

| Command | Path exercised |
| --- | --- |
| `make eval` | Tokenizer, int8 C engine, decoded call |
| `make eval-device` | Tokenizer, fixed-point forward pass, decoded call, C solver, final answer |

Recorded results for the 66-relation registry:

| Measurement | Result |
| --- | --- |
| Int8 C engine, 1,000 held-out problems | 91.6% exact calls, 2.9% wrong relation |
| Int8 C engine, 2,000 held-out problems | 91.8% exact calls, 3.3% wrong relation |
| PyTorch versus C logits | Maximum difference `7.4e-07`, identical argmax |
| Export error | Maximum per-group error `5.1e-4` |

The acceptance gates are at least 80% exact calls and at most 5% wrong
relation. Both pass. The 1,000-problem sample is what `gate.sh` runs; the
2,000-problem figure is a diagnostic and shows the rate is stable rather than
an artifact of the sample.

Accuracy is not uniform across relations. `sig_figs` is the weakest at 80%
exact, and `proj_range` accounts for the largest share of wrong-relation
cases. See `status.md` for the current gap list.

## Limitations

- **Synthetic wording.** The phrasing banks are broader than one template per relation, but they are still a closed set. Performance on wording outside the bank is the main unknown.
- **66 relations.** Anything outside the set is unsupported, even when it is mathematically simple.
- **Digit copying.** The dominant observed failure is copying a plausible number from a distractor rather than choosing the wrong relation. The grounding check refuses such calls, which trades coverage for correctness.
- **No derivation.** The model emits one call. It cannot explain intermediate steps or handle a problem that needs several calls.
- **Exactness depends on the solver.** Precalculus and calculus answers use exact rational arithmetic. Physics and chemistry answers are formatted to four significant digits, as their relation definitions specify.
- **Fixed-point drift.** Saturation is counted and parity tests are tight, but the full pipeline has not been exercised over long sessions or at temperature extremes.
- **Not on hardware yet.** The runtime is cross-compiled for `arm926ej-s` and tested on the host. End-to-end timing, memory pressure, and power behavior on a physical calculator remain unmeasured.
- **Small capacity.** 5.25M parameters is enough for a fixed set of short prompts. It is not a general language model, and increasing the relation count or phrasing variety will need more data and may need more parameters.
