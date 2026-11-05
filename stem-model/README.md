# stem-model

The on-device STEM pipeline for NdlessAI. A word problem goes in, the model emits one exact solver call, and the C runtime computes the answer.

Start with the top-level [README](../README.md) for the project overview and [docs/architecture.md](../docs/architecture.md), [docs/model.md](../docs/model.md), and [docs/development.md](../docs/development.md) for the design, model card, and build instructions.

```sh
make data
make train
make export
make host
make eval
make eval-device
make test
```

## Files

| Path | Purpose |
| --- | --- |
| `relations.py` | The call contract: name, subject, parameter names, generator, exact solver, and exactness flag |
| `surface.py` | Eight or more phrasings per relation; every fifth index is held out |
| `distractors.py` | One plausible same-unit extra quantity per relation |
| `gen_traces.py` | Question to call to answer, with each row executed as it is written |
| `make_dataset.py` | Train, validation, and held-out splits, plus tokenizer training |
| `tokenizer.py` | Byte-level BPE, 512 tokens, relation names as single tokens |
| `train.py` | Six-layer decoder with RoPE and a loss masked to call tokens |
| `export_int8.py` | Per-channel int8 quantization with group size 32 and Q24 scales |
| `export_model.py` | Checkpoint to `weights.bin` plus reference logits |
| `export_tokenizer.py` | Regenerates the embedded tokenizer tables in `runtime/bpe.h` |
| `check_tokenizer_sync.py` | Fails when the checkpoint and tokenizer header disagree |
| `evaluate.py` | Scores the int8 C engine on validation rows |
| `evaluate_device.py` | Scores the full device path, including BPE and C solvers |
| `runtime/` | C11 BPE, fixed-point forward pass, NSM1 loader, solvers, and entry points |
| `tests/` | Pipeline tests, including golden answers and logit parity |
| `PLAN.md` | Hardware measurements, model sizing, training plan, and acceptance gates |
| `QUICKSTART.md` | Short host workflow and offline notes |

## Status

The data contract, tokenizer, training, quantization, C inference engine, fixed-point path, and evaluation harness are implemented. The model is 5,246,208 parameters; the exported weights are 5.91 MB; recorded validation results are 95.0% exact calls for the int8 engine over 60 problems and 93.8% exact calls for the full device path over 80 problems.

The runtime is not yet packaged as a `.tns` application, and no checkpoint or weights file is committed.
