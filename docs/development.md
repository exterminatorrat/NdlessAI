# Development

The model pipeline is self-contained under `stem-model/`. The upstream Ndless trees are only needed when you want to build the loader, installers, or SDK samples.

## Prerequisites

- Python 3 and [`uv`](https://docs.astral.sh/uv/)
- `make` and a C11 compiler
- Optional: a CUDA or Apple-silicon GPU for faster training
- Optional: `arm-none-eabi-gcc` for the cross-compile check
- Optional: the Ndless toolchain for `.tns` builds (see [upstream Ndless](upstream-ndless.md))

## Host workflow

Run every command from `stem-model/`:

```sh
cd stem-model

make data
make train
make export
make host
make eval
make eval-device
make test
```

| Target | Effect |
| --- | --- |
| `make data` | Writes `data/train.jsonl`, `data/val.jsonl`, `data/held_out.jsonl`, `data/tokenizer.json`, and `data/meta.json`. Defaults: 200,000 train rows, 4,000 validation rows, 2,000 held-out-wording rows, 50% distractors. |
| `make train` | Trains the default `d=256`, 6-layer model and writes `model.pt`. |
| `make export` | Quantizes `model.pt` into `weights.bin`, writes `ref.npy`, and regenerates the BPE tables in `runtime/bpe.h`. |
| `make host` | Builds `runtime/infer_host`, `runtime/main_device`, and the C test binaries. |
| `make eval` | Re-exports, then scores the int8 C engine on validation rows. |
| `make eval-device` | Scores the full device path, including BPE, fixed point, and solvers. |
| `make test` | Runs `tests/test_pipeline.py` through `uv`. |

The generated files are ignored by git: `data/`, `model.pt`, `weights.bin`, `ref.npy`, `traces.jsonl`, and the compiled binaries.

## Tests

`make test` is the broad safety net. It checks:

- Golden answers for every relation, computed independently of the generator.
- Exact formatting for integers, rationals, decimals, and polynomials.
- Call round-trips through the Python executor.
- A distractor template and at least eight phrasings for every relation.
- No held-out phrasing leaking into the training split.
- Quantization error and rejection of dimensions that are not divisible by the group size.
- Tokenizer round-trips, reserved ids, and the vocabulary floor.
- Window packing that never crosses a row boundary.
- A short training run that reduces loss.
- C versus PyTorch logits, and tokenizer-header synchronization when data is present.

The C binaries have their own checks:

```sh
make -C runtime
./runtime/test_solvers
./runtime/test_bpe
./runtime/test_fixed weights.bin
```

`runtime/test_fixed` and the Python parity test need `weights.bin`; they skip or fail clearly when it is absent. `runtime/crosscheck.sh` needs `arm-none-eabi-gcc` and exits 77 when the compiler is missing:

```sh
cd runtime
sh crosscheck.sh
```

It compiles the device path for `arm926ej-s`, links a test ELF, and fails if the hot objects reference soft-float helpers.

## Building the calculator app

`stem-model/device` builds the Ndless application itself, which is a separate step from `runtime/crosscheck.sh`: that one links the runtime, this one links the whole app including `main.c`.

You need the Ndless SDK's ARM toolchain on `PATH` (`nspire-gcc`, `nspire-as`, `nspire-ld`, `genzehn`, `make-prg`), and the model artefacts the build packages. Neither is committed, so build them first:

```sh
cd stem-model
make data && make export
cd device && make
```

That writes two files, and both must be copied into the same folder on the calculator because the app looks for its model beside itself:

| File | What it is |
| --- | --- |
| `stemhelp.tns` | The application. |
| `weights.bin.tns` | `../weights.bin` renamed, because TI's transfer software only carries `.tns`. 5.9 MB. |

Without `weights.bin.tns` the app still answers typed arithmetic and plots graphs, but not word problems. `make` also refreshes `runtime/bpe.h` from `data/tokenizer.json` first, so a stale tokenizer cannot ship by accident.

Nothing here has been run on a calculator yet: the app has been cross-compiled and measured on the host path only.

## Adding a relation

A relation touches the Python contract, the phrasing bank, the distractor bank, the C solver, and the tests. Keep the arity fixed and keep the call fields whitespace-separated.

1. **Define the relation** in `relations.py` with `@relation(name, subject, params, exact=...)`. Implement `gen(rng)` to return `(params, question)` and `solve(*params)` to return the formatted answer. Registering it updates `RELATIONS` and the tokenizer's reserved ids.
2. **Add a distractor** in `distractors.py`, both a sentence template and a numeric pool. Every relation must have one.
3. **Add at least eight phrasings** in `surface.py`, plus the argument names in `_ARG_NAMES`. Every phrasing must contain every argument and preserve its role. The held-out rule is automatic: index `4 mod 5`.
4. **Port the solver** to `runtime/solvers.c` and add it to the dispatch chain in `solvers_solve`. Use the existing rational and decimal helpers; do not introduce floating point into the hot path.
5. **Add test vectors** to `runtime/test_solvers.c` and a golden answer to `tests/test_pipeline.py`.
6. **Regenerate and verify** with `make data`, `make train`, `make export`, `make host`, `make test`, and `make eval-device`.

Changing the tokenizer invalidates a checkpoint. Always export and train with the same `data/tokenizer.json`, and commit the regenerated `runtime/bpe.h` alongside the change. `check_tokenizer_sync.py` is the guard against a mismatch.

## Tuning

Training flags are documented by `train.py --help`. The important constraints are:

- `--dim` must be divisible by `--heads` and by 32 for group quantization.
- `--seq` must be long enough for the longest question plus the call. The default is 512.
- The loss must stay masked to call tokens; the trainer already does this.
- `--max-minutes` stops a run early and saves the checkpoint.

Quick sanity check before a full run:

```sh
uv run --with torch --with numpy python train.py \
    --traces data/val.jsonl --tokenizer data/tokenizer.json \
    --steps 1 --batch 8 --samples 5 --out /tmp/ignore.pt
```

The printed samples show expected versus generated calls. Exact call match is the number that matters, not loss.

## Offline work

The first run of each `uv` command downloads packages. After that, `uv` can run fully offline:

```sh
uv run --offline --with python --with torch --with numpy --with sympy ...
```

Dataset generation and training make no network calls of their own. `make data` needs SymPy; training needs PyTorch and NumPy.

## Ndless trees

The root `Makefile` builds the upstream Ndless SDK and installer, and it expects the git submodules:

```sh
git submodule update --init --recursive
make
```

That build is independent of the model pipeline. `ndless-sdk/samples/int8bench/` is the hardware benchmark that measured int8 dot-product cost; see its README for the manual stopwatch procedure, since `clock()` is stubbed in the SDK.

## Troubleshooting

| Symptom | Cause and fix |
| --- | --- |
| `cols 256 not divisible by group 32` | `--dim` must be a multiple of 32. |
| `--dim not divisible by --heads` | Pick matching values, for example 256 and 8. |
| `weights.bin missing` | Run `make export` before `make eval-device` or `runtime/test_fixed`. |
| Accuracy collapses to zero | The checkpoint and `bpe.h` came from different tokenizer runs. Re-export and retrain together. |
| `clock()` prints nonsense on the calculator | Expected; `_times()` is stubbed in the SDK. Use the int8bench stopwatch procedure. |
| `crosscheck.sh` says skip | `arm-none-eabi-gcc` is not on `PATH`; install the ARM toolchain to enable it. |
