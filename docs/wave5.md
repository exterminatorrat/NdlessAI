# Wave 5: making the question cheap to type

The calculator has one input line and about fifty keys. A student who wants a
friction force has to type thirty characters of English, and the decoder then
has to read them back correctly. Both halves of that are expensive, and only one
of them is a model problem.

So wave 5 is about entry, not accuracy. The first piece is **a typed call**,
where the student names the relation and gives its numbers, and nothing is
decoded at all.

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

## Solve by formula

The second piece: **MENU → Solve by formula → subject → relation → one prompt
per argument**, and the answer arrives with its working.

```
SOLVE 1/3   algebra   calculus   chemistry   logs   modern   ...
SOLVE 2/3   friction_force   free_fall   grav_force   ...
SOLVE 3/3   friction_force
            1. mu  = 0.4
            2. N   = _
```

Every relation already declared its arguments — `friction_force` is
`["mu", "N"]` — so the prompts are the catalogue's own names rather than a
second list to keep in sync. They travel in `reltable.h`, the table the decoder
already masks from, so a relation cannot be answerable in English and missing
from the picker: adding one adds it to both, or the export gate fails.

This is the D8 "template keypad" the roadmap asked for, and it is what makes the
catalogue reachable by a student who does not know a relation is called
`friction_force`. Like the typed call, it needs no model.

One entry path is left, and it is the most expensive:

| Next | What it is | Why it is last |
| --- | --- | --- |
| **Sentence templates** | A phrasing from the bank with its numbers blanked: "A mass of __ kg accelerates at __ m/s^2" — fill the blanks with the arrows | It keeps natural language, but it needs template storage on the device and a blank-navigation UI, and everything after the blanks still routes through the decoder |

The order is deliberate: each step removes more typing per unit of change than
the one after it. Typed calls remove the decoder for the questions a student can
name; the picker removes the need to know the name; templates remove the need to
know the phrasing.

## What it does not fix

A typed call still needs the relation's name, and a picker still needs the right
subject and relation to be guessable from a list. Neither helps a student who
does not know which quantity they are after; that is a phrasing problem, and the
decoder is the only thing in the program that solves it.
