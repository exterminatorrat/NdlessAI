"""Pin the answer format of every fraction-or-percent relation.

One quantity returned as `50%` by one relation and `0.5` by another is not a
wording difference: the sentence, the value and the student's reading of it have
to agree. These tests record which format each relation uses, so a new relation
cannot quietly pick the other one and so a row that says "percent" while
returning `0.5` fails here rather than in a classroom.

The two lists are pinned deliberately rather than derived: deriving them would
let a format change pass unnoticed, which is the failure these tests exist for.
"""

import random
import re

import pytest

from relations import RELATIONS

PERCENT = {
    "mass_percent": "percent by mass",
    "percent_change": "percent change",
    "percent_dissociation": "percent ionization",
    "percent_error": "percent error",
    "percent_yield": "percent yield",
}

FRACTION = {
    "carnot": "efficiency",
    "carnot_efficiency": "efficiency",
    "mole_fraction": "mole fraction",
    "half_life_fraction": "fraction decayed",
    "probability_single": "probability",
    "submerged_fraction": "fraction submerged",
}

BARE = re.compile(r"-?\d+(?:\.\d+)?(?:e[-+]?\d+)?")


def answers(name, count=6):
    relation = RELATIONS[name]
    out = []
    for seed in range(count):
        args, _ = relation.gen(random.Random(seed))
        out.append(relation.solve(*args))
    return out


def test_percent_relations_return_a_percent_sign():
    for name in PERCENT:
        for text in answers(name):
            assert text.endswith("%"), (name, text)


def test_fraction_relations_return_a_bare_fraction():
    for name in FRACTION:
        for text in answers(name):
            assert not text.endswith("%"), (name, text)
            assert BARE.fullmatch(text.strip()), (name, text)
            assert 0.0 <= float(text) <= 1.0, (name, text)


def test_a_relation_named_for_a_percent_returns_a_percent():
    # `percent_of` is the exception and is not a mistake: it takes a percentage
    # and returns the amount that percentage is worth, so its answer is a
    # quantity in the same units as its input, not a percentage.
    for name in RELATIONS:
        if name.startswith("percent_") and name != "percent_of":
            assert name in PERCENT, name
        if name == "mass_percent":
            assert name in PERCENT, name


def test_a_relation_named_for_a_fraction_returns_a_fraction():
    for name in RELATIONS:
        if name.endswith("_fraction") or name in ("probability_single",
                                                  "submerged_fraction"):
            assert name in FRACTION, name


def test_the_two_lists_are_disjoint_and_name_relations_that_exist():
    assert not set(PERCENT) & set(FRACTION)
    for name in list(PERCENT) + list(FRACTION):
        assert name in RELATIONS, name
    for name, quantity in PERCENT.items():
        assert quantity, name
    for name, quantity in FRACTION.items():
        assert quantity, name


def test_one_quantity_is_expressed_in_one_format():
    """ELV-77, fixed: two names for efficiency, one format, recorded as a synset.

    The decision was to keep both names — retiring one costs a token the decoder
    can emit and a phrase a question can map to, and buys only the removal of a
    synonym — and to unify the format on the fraction. This fails if someone
    re-adds a percent variant.
    """
    from surface.spec import SYNONYMS

    assert SYNONYMS["carnot"] == "carnot_efficiency"
    assert "carnot" in FRACTION and "carnot_efficiency" in FRACTION
    assert "carnot" not in PERCENT and "carnot_efficiency" not in PERCENT
    for left, right in SYNONYMS.items():
        assert left in RELATIONS and right in RELATIONS
        assert (left in FRACTION) == (right in FRACTION), (left, right)
        assert (left in PERCENT) == (right in PERCENT), (left, right)


def test_a_synset_answers_the_same_quantity_the_same_way():
    """Two names for one quantity must agree on a value, not merely on a format."""
    left = RELATIONS["carnot"].solve(800, 200)
    right = RELATIONS["carnot_efficiency"].solve(200, 800)
    assert left == right, (left, right)
