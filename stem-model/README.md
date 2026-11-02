Everything the on-device model has to learn is the mapping

    question text  ->  CALL <relation> <arg> <arg> ...

It never emits a number. Numbers come from `relations.py`, executing on the
calculator in exact arithmetic, which is why an 11M-parameter model is enough:
the mathematics lives in code, and only the language understanding lives in
weights.

Fewer tokens is also the only real speed lever. A call is ~24 tokens, so at the
measured ~3.5 tok/s that is about 7 s per problem, while a narrated derivation
would be 300+ tokens and a minute and a half.

    relations.py      the contract: fixed arity, so the grammar is trivial
    gen_traces.py     question -> call -> answer, every row executed to check it
    distractors.py    one plausible same-unit extra quantity per row
    export_int8.py    per-channel int8 + fixed-point scales, checked against float
    PLAN.md           the full plan: measured hardware, model size, training, gates

Run it:

    uv run --with sympy python gen_traces.py --n 400 --out traces.jsonl

Stage 1, where this stands: a verified data contract. Every generated row is
executed through `run_call` and compared against the relation's own solver, so
the call string is proven to round-trip and the answer is computed rather than
hand-written.

Stage 2, done: distractors. Each question can carry one extra quantity of the
same unit, drawn from a per-relation pool so it stays plausible. Published work
shows a small model drops from 116/120 to 61/120 on exactly this, so it is the
highest-value data work available and it costs no parameters.

Stage 3: tokenizer. Byte-level or a 512-1024 token domain BPE over the call
vocabulary. Small vocab keeps the embedding table from eating the 21.5 MiB
budget.

Stage 4: training off-device. A llama2.c-style model at d=256..352, 6 layers,
trained on these traces plus generic text, on a normal GPU. Nothing about it
needs the calculator.

Stage 5: export. Per-channel int8, group size 32, scales stored as fixed-point
Q16 so the calculator never touches a float. Weights are emitted as packed
`uint32_t` words in the same layout `int8bench` reads.

    uv run --with numpy python export_int8.py --out weights.h

Stage 6: on-device runtime. Prompt template, grammar-constrained decoding so
only valid call tokens can be sampled, then dispatch into the ported relation
solvers C-side.

Stage 7: evaluation that matters. Exact-match on held-out generated sets, with
and without distractors, reported as accuracy and decline rate rather than
loss.

The benchmark already settled the hardware question, so stages 4-6 are ordinary
work. The risk is concentrated in stage 2 and stage 7: whether language
understanding at this capacity is good enough, which is an evaluation question,
not a modelling one.
