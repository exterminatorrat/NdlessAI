# Tier 1 sections: the loop

`TIER1_PLAN.md` says what to build. This file says how to build it in
repeatable units that survive context loss.

## The loop

One section is one relation. Each is a complete, verifiable, committable unit.

    read PROGRESS.md
    run gate.sh                    -> must pass before starting
    take first pending section
    follow the recipe below
    run gate.sh                    -> must pass before committing
    update PROGRESS.md, commit
    repeat

If context is lost mid-section, the ledger still says `pending` and the gate
still passes, so the next agent restarts that section cleanly. Nothing is
half-done in a way that looks finished.

## The recipe

Every relation follows the same five steps. `NAME` is the relation name from
`TIER1_PLAN.md` section 3.

### Step 1: relations.py

Add one class. `gen()` produces parameters and a question; `solve()` returns
the answer string.

```python
@relation("NAME", "SUBJECT", ["arg1", "arg2"], exact=False)
class Name:
    @staticmethod
    def gen(rng):
        a = rng.randint(1, 20)
        b = rng.choice([2, 3, 4, 5])
        return [a, b], f"Question text with {a} and {b}."

    @staticmethod
    def solve(a, b):
        return f"{dec(sp.Float(a) / sp.Float(b))} UNIT"
```

Rules:

- `exact=True` only when the answer is a rational or integer. Then use `fmt()`.
- `exact=False` for physical quantities. Then use `dec()`, which is four
  significant digits with trailing zeros stripped.
- Every parameter must appear literally in the question text, or the guard
  will reject the call.
- `gen()` must never produce a degenerate case: no division by zero, no
  negative square root, no log of a non-positive number.

### Step 2: surface.py

Add 8-14 phrasings to `TEMPLATES["NAME"]`. Every fifth is held out
automatically by `is_held_out()`.

Also add the argument names to `_ARG_NAMES`, in the same order as the
`@relation` params list. `render()` zips them together, so a missing entry is
a `KeyError` at dataset build time.

Rules:

- Vary the verb, word order, and notation. "Find", "compute", "what is",
  "determine" are different phrasings, not the same one.
- Keep the same numbers in the same roles in every phrasing.
- Use `{argname}` placeholders matching the names in `@relation`.
- Do not write fewer than 8. Measured: 4 phrasings gave 35% held-out, 8-12
  gave 57%.

### Step 3: runtime/solvers.c

### Step 2b: distractors.py

Add one entry to `TEMPLATES` and, if the relation has a natural pool of
plausible wrong values, one to `POOLS`. The template takes `{n}`, a number
that is not one of the relation's arguments.

```python
"NAME": "A second problem in the same set asks for {n}.",
```

`tests/test_pipeline.py::test_every_relation_has_a_distractor_template` fails
without this. The recipe originally omitted this step; the gate caught it.

### Step 3: runtime/solvers.c

Add one `solve_NAME()` function and one dispatch branch.

The function must produce a **byte-identical** string to Python's `solve()`.
That is the whole point of the port: it is what makes the C verifiable.

```c
static int solve_NAME(const char *const *args, char *out, size_t outlen)
{
    rat a, b, r;
    int rc;
    char f1[48], buf[64];

    if ((rc = parse_rat(args[0], &a, NULL)) != 0 ||
        (rc = parse_rat(args[1], &b, NULL)) != 0)
        return rc;
    if (b.n == 0)
        return -4;
    if ((rc = rat_div(&r, &a, &b)) != 0)
        return rc;
    if ((rc = dec4_render(r.n, r.d, f1, sizeof(f1))) != 0)
        return rc;
    snprintf(buf, sizeof(buf), "%s UNIT", f1);
    out_set(out, outlen, buf);
    return 0;
}
```

Dispatch branch, in `solvers_solve()`:

```c
} else if (strcmp(name, "NAME") == 0) {
    if (nargs == 2)
        rc = solve_NAME(args, out, outlen);
    else
        rc = -2;
```

Available helpers: `parse_rat`, `rat_add`, `rat_sub`, `rat_mul`, `rat_div`,
`rat_cmp`, `dec4_render` (four significant digits), `fmt_rat` (exact rational
or integer), `out_set`. Use them; do not reimplement.

### Step 4: runtime/test_solvers.c

Generate the cases from Python. Never hand-write expected values.

Write a throwaway script that imports `RELATIONS`, calls `gen()` 200 times for
the new relation, and prints one C initialiser per case in the existing table
format. Paste the output into the case table. The test then compares the C
output against those strings byte for byte.

### Step 5: verify and commit

```bash
sh stem-model/gate.sh
```

All gates must pass. Then update `PROGRESS.md` and commit:

```bash
git add -A
git commit -m "Add NAME relation"
```

## Section list

Ordered by dependency. Within a batch, order does not matter.

| Section | Relation | Batch | Depends on |
|---|---|---|---|
| S0 | infrastructure | — | done |
| A1-A8 | trig | A | S0 |
| B1-B7 | logs and exponentials | B | S0 |
| C1-C8 | algebra | C | S0 |
| D1-D12 | physics | D | S0 |
| E1-E10 | chemistry | E | S0 |
| F1-F5 | units | F | S0 |
| G1 | retrain and evaluate | — | all batches |
| G2 | hardware test | — | G1 |

## Parallelism

Batches A-F are independent: different relation names, no shared state. But
they all edit the same three files, so **merges will conflict** if two run at
once.

- **Serial** (simplest): one batch at a time, no conflicts. Roughly 50 focused
  sessions for 50 relations.
- **Parallel with rebase**: each subagent on its own branch, parent rebases
  and resolves the three-file conflict per batch. Faster, but the conflict
  resolution is manual and error-prone.

Start serial. Switch only if throughput becomes the real constraint.

## Failure handling

| Gate fails | Action |
|---|---|
| solvers match Python | The C string differs. Print both, find the formatting difference. Usually `dec4_render` vs `fmt_rat`. |
| guard cases | A parameter is missing from the question text, or a sign is wrong. Fix `gen()`. |
| tokenizer round-trip | A phrasing contains a character the tokenizer cannot encode. |
| ARM cross-compile | A new solver used a libm function. Use the integer helpers. |
| held-out < 80% | Add phrasings. Do not add parameters. |
| wrong relation > 5% | Two relations are too similar. Make their phrasings more distinctive. |

## What not to do

- Do not add a relation without 8 phrasings. It will score well in training
  and fail in use.
- Do not hand-write C test expectations. Generate them from Python.
- Do not mark a section done without `gate.sh` passing.
- Do not change the model architecture during Tier 1. The model is not the
  bottleneck.
- Do not batch more than one relation per commit. A failing gate should point
  at one relation, not ten.
