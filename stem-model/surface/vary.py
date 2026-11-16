"""Wording families: rewrite a rendered question so its surface is not fixed.

A relation with ten templates is still ten sentences. This model emits a call,
so the sentence is the whole interface: an opener, notation or spelling it has
never seen is a straight miss, which reads as a capability failure even when the
formula is right. Ten templates per relation is memorising ten sentences.

This module multiplies those templates through families that keep every number
and its role intact, so the call does not change:

  notation   "15%"  <->  "15 percent"  <->  "15 per cent"
  money      "80 dollars"  <->  "$80"
  spelling   UK  <->  US  ("litres" / "liters", "metres" / "meters")
  opener     "Find the average ..."  <->  "Calculate / Determine / Give ..."

Training draws from the common choices. Two choices stay out of training on
purpose and only appear in the wording-family eval cohort: the notation
"per cent" and the opener "Work out". That cohort therefore measures transfer
to wording the model has never memorised, which is the failure that matters
here - a right formula written in a sentence the model has not seen reads as a
wrong answer. Spelling is varied both ways but is not held out, because US forms
("grams", "meters") are already in the templates.
"""

import re

TRAIN_OPENERS = ("Find", "Calculate", "Compute", "Determine", "Give")

HELD_OUT_OPENER = "Work out"

IMPERATIVE = re.compile(r"\b(Find|Calculate|Compute|Determine|Give|Solve|"
                        r"Evaluate|Convert)\b(?!.*\bto\b)")

PERCENT_ATTACHED = re.compile(r"(\d+(?:\.\d+)?)%")
PERCENT_WORD = re.compile(r"(\d+(?:\.\d+)?)\s+percent\b")

MONEY_WORD = re.compile(r"(\d+(?:\.\d+)?)\s+dollars?\b")

UK_TO_US = (
    ("kilometres", "kilometers"),
    ("centimetres", "centimeters"),
    ("millimetres", "millimeters"),
    ("litres", "liters"),
    ("metres", "meters"),
    ("grammes", "grams"),
)


def _notation(text, rng, held_out):
    if held_out:
        text = PERCENT_ATTACHED.sub(r"\1 per cent", text)
        return PERCENT_WORD.sub(r"\1 per cent", text)
    if rng.random() < 0.5:
        return PERCENT_ATTACHED.sub(r"\1 percent", text)
    return PERCENT_WORD.sub(r"\1%", text)


def _money(text, rng):
    if rng.random() < 0.5:
        text = MONEY_WORD.sub(r"$\1", text)
    return text


def _spelling(text, rng, held_out):
    if held_out:
        for uk, us in UK_TO_US:
            text = re.sub(rf"\b{uk}\b", us, text)
    return text


def reserved_families(text):
    """Which wording choices in `text` never appear in a training row."""

    found = []
    if "per cent" in text:
        found.append("per-cent")
    if text.startswith(HELD_OUT_OPENER):
        found.append("work-out")
    return found


INTERROGATIVE = re.compile(r"^(What is|What\'s|How much is|What was|What are)\b")


def _opener(text, rng, held_out):
    if held_out:
        m = INTERROGATIVE.match(text)
        if m is not None:
            rest = text[m.end():]
            if rest.endswith("?"):
                rest = rest[:-1]
            return f"{HELD_OUT_OPENER}{rest}."
        m = IMPERATIVE.match(text)
        if m is not None:
            return HELD_OUT_OPENER + text[m.end():]
        return text
    m = IMPERATIVE.match(text)
    if m is None:
        return text
    return rng.choice(TRAIN_OPENERS) + text[m.end():]


def train_variant(text, rng):
    """A surface for training: common choices only, so the held-out stays held out."""

    text = _notation(text, rng, held_out=False)
    text = _money(text, rng)
    text = _opener(text, rng, held_out=False)
    return text


def held_out_variant(text, rng):
    """A surface for the wording-family cohort: choices no training row carries."""

    text = _notation(text, rng, held_out=True)
    text = _money(text, rng)
    text = _spelling(text, rng, held_out=True)
    text = _opener(text, rng, held_out=True)
    return text


def grounded_params(text, params):
    """Every argument still appears in the sentence, so the call stays derivable."""

    return all(str(p) in text for p in params)
