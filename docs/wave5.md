# Wave 5: making the question cheap to type

The calculator has one input line and about fifty keys. A student who wants a
friction force has to type thirty characters of English, and the decoder then
has to read them back correctly. Both halves of that are expensive, and only one
of them is a model problem.

So wave 5 is about entry, not accuracy. The first piece is in this branch: **a
typed call**, where the student names the relation and gives its numbers, and
nothing is decoded at all.

## What a typed call is

```
fri 0.4 500        ->  CALL friction_force 0.4 500    ->  200 N
kinematic_v 9 2 8  ->  CALL kinematic_v 9 2 8          ->  25 m/s
kv 9 2 8           ->  the same relation by its initials
```

The answer pane already printed `CALL friction_force 0.4 500` under every word
problem, so the syntax was on screen all along; this makes it an input. Three
rules keep it from being surprising:

1. **A name token and nothing but numbers.** A sentence cannot match, because
   every token after the first must be numeric. `integrate(x^2)` cannot match
   either. Arithmetic is tried first, so `2+2` is untouched.
2. **The full name always works; shorter has to be unique.** The initials of
   the name's parts work when only one relation has them (`kv`, `ov`, `ke`), and
   otherwise the student types as much of the name as it takes, never fewer than
   three letters. `ff` is refused because `free_fall` and `friction_force` both
   claim it — a mnemonic that means two things is worse than no mnemonic.
3. **A wrong number of arguments is refused, not guessed.** `fri 0.4` shows the
   call it understood and says it could not read the question. It is not handed
   to the decoder, which would read a sentence that was never one.

Two properties fall out of this and both matter more than the typing saved:

- **It needs no model and no weights file.** A calculator with only
  `stemhelp.tns` copied on can answer all 136 relations this way; the arithmetic
  path already worked without weights, and now the whole catalogue does. The
  model is only needed to read English.
- **It is exact and instant.** One comparison and one solver call, against a
  decode that costs seconds and can be wrong.

The other entry paths, neither built yet, in the order they earn their keep:

| Next | What it is | Why it is second |
| --- | --- | --- |
| **Solve by formula** | MENU → subject → relation → one prompt per argument, labelled from the formula (`f = mu*N` asks for `mu`, then `N`), then the solver answers | The data already exists: every relation declares `params` and its step line, and `export_steps.py` can carry them into the C table. It costs a UI mode rather than a parser, and it is the D8 "template keypad" the roadmap asked for |
| **Sentence templates** | A phrasing from the bank with its numbers blanked: "A mass of __ kg accelerates at __ m/s^2" — fill the blanks with the arrows | Keeps natural language, but it needs template storage on the device and a blank-navigation UI, and it still routes through the decoder for everything after the blanks |

The order is deliberate: each step removes more typing per unit of change than
the one after it. Typed calls remove the decoder entirely for the questions a
student can name; the picker removes the need to know the name; templates remove
the need to know the phrasing.

## What it does not fix

A student who does not know the relation is called `friction_force` still cannot
use a typed call, which is exactly what the picker is for. And a shorthand
answer prints the working from `steps.c`, so it is as good as the step template
for that relation and no better — 123 of 136 relations have one.
