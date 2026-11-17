import pytest

from relations import RELATIONS
from surface import numbers, spec

CHEMISTRY = sorted(name for name, relation in RELATIONS.items()
                   if relation.subject == "chemistry")


def test_every_chemistry_relation_has_a_spec():
    missing = [name for name in CHEMISTRY if name not in spec.SPECS]
    assert missing == [], f"no spec for {missing}"


def test_spec_slots_are_the_relation_parameters():
    for name, entry in spec.SPECS.items():
        assert list(entry.slots) == list(RELATIONS[name].params), name


def test_confusable_relations_exist():
    for name, entry in spec.SPECS.items():
        for other in entry.confusable:
            assert other in RELATIONS, f"{name} names {other}"


def test_same_dimension_pairs_are_computed_not_declared():
    assert spec.SPECS["boyles"].same_dim == (("V1", "V2"),)
    assert spec.SPECS["molarity"].same_dim == ()
    assert spec.SPECS["combined_gas"].same_dim == (("P1", "P2"), ("T1", "T2"))


def test_every_slot_has_a_role_a_dimension_and_a_unit():
    for name, entry in spec.SPECS.items():
        for param, slot in entry.slots.items():
            assert slot.role, (name, param)
            assert slot.dim, (name, param)
            assert slot.unit is not None, (name, param)
        assert entry.answer.role and entry.answer.dim
        assert entry.answer_words and all(word for word in entry.answer_words)


@pytest.mark.parametrize("name", ["molarity", "boyles", "ph", "percent_dissociation"])
def test_every_answer_unit_is_writable(name):
    entry = spec.SPECS[name]
    assert entry.answer.unit == "" or " " not in entry.answer.unit


def test_canon_refuses_a_value_that_needs_an_exponent():
    assert numbers.canon(0.00001) is None
    assert numbers.canon(1e9) is None
    assert numbers.canon(87.0) == "87"
    assert numbers.canon(0.0625) == "0.0625"
    assert numbers.canon(-4) == "-4"
    assert numbers.canon(0) == "0"


def test_with_unit_does_not_leave_a_trailing_space():
    assert numbers.with_unit("0.2", "mol") == "0.2 mol"
    assert numbers.with_unit("3", "") == "3"
