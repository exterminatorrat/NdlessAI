# Paraphrase corpus

`paraphrase_cases.jsonl` holds five hand-written phrasings for each of the 84
relations, so wording — not coverage — can be measured. One JSON object per
line:

    {"relation": "kinematic_v", "args": [20, -4, 3], "question": "...", "split": "eval"}

- `args` are the relation's parameters in declared order, as integers or
  decimals the C call path can parse.
- `question` is how a student would type it: story context, the quantity named
  late, units spelled out, connected speech, and an occasional irrelevant
  number. The wording deliberately avoids the phrasings in `surface/`, because
  those are what the model trains on; no case is a template with a number
  swapped.
- `split` is `train` (two per relation) or `eval` (three). Within a relation,
  the two cases whose `sha256("relation|question")` sorts lowest are `train`.
  The rule is fixed, so the split cannot be tuned case by case. The eval rows
  are the only honest measurement.

Every row must satisfy two properties, and `evaluate_paraphrase.py` re-checks
both when it loads the file:

1. `relations.RELATIONS[relation].solve(*args)` returns an answer without
   raising.
2. Every argument appears in the question under the runtime's grounding rule
   (`pipeline_args_grounded` in `runtime/pipeline.c`): the digits must match on
   a full token, signs must agree, and `0` and `1` are exempt. A negative
   argument therefore has to be written with a minus in the text, so
   deceleration cases say `a = -4 m/s^2` rather than only "decelerates".

Score them with `python3 evaluate_paraphrase.py`, which reports each split
separately; `--check-corpus` validates the file without a binary or weights.
