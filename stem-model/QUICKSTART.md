# Quickstart

Four commands, one machine. You need Python, `uv`, and `make`. A GPU is
optional: the whole model trains on a laptop CPU, just slower.

    make data      # generate the dataset and tokenizer (a few minutes)
    make train     # train the model (about an hour on CPU, minutes on a GPU)
    make export    # quantize to int8 and write weights.bin
    make host      # build the C engine and check it against PyTorch
    make eval      # score the int8 C engine on held-out problems
    make eval-device  # score the full on-device path: BPE + fixed point + solvers

`make test` runs the full suite, including the test that the C engine's logits
match PyTorch to within 1e-4.

## What each step produces

| Step | Output | Size |
|---|---|---|
| `make data` | `data/train.jsonl`, `data/val.jsonl`, `data/tokenizer.json`, `data/meta.json` | ~40 MB |
| `make train` | `model.pt` | 21 MB |
| `make export` | `weights.bin` | ~6 MB |

## On a GPU

    uv run --with torch --with numpy --with sympy python train.py \
        --traces data/train.jsonl --tokenizer data/tokenizer.json \
        --batch 64 --steps 8000 --out model.pt

`--batch` is safe to raise: the model is 5.2M parameters, so weights plus
optimizer state are under 200 MB and a 12 GB card is mostly idle.

## Checking the model before you ship it

    uv run --with torch --with numpy python train.py \
        --traces data/val.jsonl --tokenizer data/tokenizer.json \
        --steps 1 --batch 8 --samples 5 --out /tmp/ignore.pt

The samples printed at the end show expected-vs-generated calls for held-out
problems. That is the number that matters: exact call match, not loss.

## Troubleshooting

- `AssertionError: cols 256 not divisible by group 32` — `--dim` must be a
  multiple of 32. Use 256.
- `--dim` not divisible by `--heads` — pick matching values, e.g. 256 and 8.
- `clock()` prints nothing sensible on the calculator — that is expected; see
  `ndless-sdk/samples/int8bench/README.md`.

## Running offline

Only the first run of each command needs a network, because `uv` fetches
Python packages the first time it sees them. After that everything is local.

| Step | Network needed |
|---|---|
| `git clone` | once |
| first `uv run --with sympy ...` | once, to fetch sympy |
| first `uv run --with torch --with numpy ...` | once, to fetch torch and numpy |
| every run after that | none |

To prove it, or to work on a plane, add `--offline` to any command:

    uv run --offline --with python --with sympy --with torch python train.py ...

`--offline` makes `uv` refuse to reach the network at all, so if a package is
missing the command fails immediately instead of hanging. Warm the cache
first by running each command once with a connection.

Dataset generation and training do not otherwise touch the network: there are
no downloads inside them and no telemetry. `make data` needs only sympy, and
`make train` needs only torch.
