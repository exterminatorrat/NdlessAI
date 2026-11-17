import json
import os
import subprocess
import sys

import pytest

from evaluate_paraphrase import arg_in_text
from relations import RELATIONS
from surface import grammar, lexicon, spec

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def test_a_bare_list_is_refused_only_where_a_reader_could_not_resolve_it():
    boyles = spec.SPECS["boyles"]
    listed = [structure for structure in grammar.structures(boyles)
              if structure.given == "list"]
    assert listed
    assert all(grammar.refusal(boyles, structure) for structure in listed)

    molarity = spec.SPECS["molarity"]
    listed = [structure for structure in grammar.structures(molarity)
              if structure.given == "list"]
    assert listed
    assert all(grammar.refusal(molarity, structure) is None for structure in listed)


def test_a_named_block_is_never_refused():
    for name, entry in spec.SPECS.items():
        for structure in grammar.structures(entry):
            if structure.names_roles:
                assert grammar.refusal(entry, structure) is None, (name, structure.id)


def test_structure_ids_are_unique_within_a_relation():
    for name, entry in spec.SPECS.items():
        ids = [structure.id for structure in grammar.structures(entry)]
        assert len(ids) == len(set(ids)), name


def test_framings_are_declared_per_relation_and_always_offer_the_bare_one():
    assert "volumes" in grammar.framings_for(spec.SPECS["mixing_concentration"])
    assert grammar.framings_for(spec.SPECS["avg_rate"]) == ("", "modifier")
    assert "modifier" not in grammar.framings_for(spec.SPECS["percent_yield"])
    assert grammar.framings_for(spec.SPECS["atoms_from_mass"]) == ("",)


def test_a_lexicon_word_never_replaces_a_precise_role():
    slot = spec.SPECS["first_law"].slots["w"]
    assert lexicon.role_words(slot, "classroom") == (slot.role,)


@pytest.mark.parametrize("name", ["molarity", "boyles", "h_from_ph", "percent_yield"])
def test_rendered_rows_ground_every_argument(name):
    entry = spec.SPECS[name]
    args, _ = RELATIONS[name].gen(__import__("random").Random(7))
    args = [int(value) if float(value).is_integer() else value for value in args]
    checked = 0
    for structure in grammar.structures(entry):
        if grammar.refusal(entry, structure):
            continue
        row = grammar.render(entry, args, structure, "exam", 0)
        assert row is not None
        for value in args:
            assert arg_in_text(value, row.text), (name, structure.id, value, row.text)
        checked += 1
    assert checked


def test_rendered_rows_have_no_doubled_space_or_placeholder():
    for name, entry in spec.SPECS.items():
        args, _ = RELATIONS[name].gen(__import__("random").Random(3))
        args = [int(value) if float(value).is_integer() else value for value in args]
        for structure in grammar.structures(entry):
            if grammar.refusal(entry, structure):
                continue
            row = grammar.render(entry, args, structure, "exam", 0)
            assert row is not None
            assert "  " not in row.text
            assert "{" not in row.text and "}" not in row.text
            assert row.text.endswith((".", "?", "!"))


def test_corpus_rows_name_a_relation_that_exists():
    path = os.path.join(HERE, "fixtures", "paraphrase_chem-generated_r6.jsonl")
    with open(path) as handle:
        rows = [json.loads(line) for line in handle if line.strip()]
    assert rows
    for row in rows:
        assert row["relation"] in RELATIONS
        assert row["relation"] in spec.SPECS
        assert row["split"] in ("train", "eval")
        assert "structure" in row


def test_contrast_rows_put_both_sides_on_the_same_numbers():
    path = os.path.join(HERE, "fixtures", "paraphrase_chem-generated_r6.jsonl")
    with open(path) as handle:
        rows = [json.loads(line) for line in handle if line.strip()]
    paired = [row for row in rows if "pair" in row]
    assert paired, "no contrast rows were generated"
    by_pair = {}
    for row in paired:
        by_pair.setdefault(row["pair"], []).append(row)
    for pair, side_rows in by_pair.items():
        args = {tuple(row["args"]) for row in side_rows}
        assert len(args) == 1, pair
        assert len(side_rows) % 2 == 0, pair


def test_the_split_is_a_function_of_the_structure_alone():
    """A structure added later cannot change an earlier structure's side."""
    import gen_wording

    entry = spec.SPECS["molarity"]
    verdicts = {structure.id: gen_wording.held_out("molarity", structure.id)
                for structure in grammar.structures(entry)}
    for structure_id, side in verdicts.items():
        assert gen_wording.held_out("molarity", structure_id) == side
    assert len(set(verdicts.values())) == 2, "the holdout splits this relation"
    first_id = sorted(verdicts)[0]
    assert gen_wording.held_out("molarity", first_id) == verdicts[first_id]


def test_ledger_names_the_pairs_it_cannot_carry():
    with open(os.path.join(HERE, "..", "docs", "wording-ledger.md")) as handle:
        ledger = handle.read()
    for pair in ("sin_eval/cos_eval", "percent_change/percent_error"):
        assert pair in ledger
    assert "ELV-77" in ledger


def test_every_spec_has_provenance_and_a_known_source():
    for name in spec.SPECS:
        registers, key = spec.PROVENANCE.get(name, spec.DEFAULT_PROVENANCE)
        assert registers, name
        assert key in spec.SOURCES, (name, key)


def test_a_modifier_that_restates_the_answer_is_refused():
    """The guard the double-decoration bug needs: 'percent yield' twice."""
    from surface import grammar as g

    entry = spec.SPECS["percent_yield"]
    g.MODIFIERS["percent_yield"] = (" For this yield, read on.", ("yield",))
    try:
        assert g.modifier_for(entry) is None
        g.MODIFIERS["percent_yield"] = (" Read on.", ("percent",))
        assert g.modifier_for(entry) is None
    finally:
        del g.MODIFIERS["percent_yield"]


def test_species_are_only_offered_where_they_cannot_change_the_answer():
    for name in spec.BLOCKED_SPECIES:
        assert name not in spec.SPECIES, name
    for name in spec.SPECIES:
        assert name in spec.SPECS, name


def test_the_physics_clusters_are_specd_with_applied_registers():
    for name in ("specific_heat", "conduction_rate", "radiation_power",
                 "gas_heat_cp", "entropy_change", "wave_speed",
                 "standing_wave_freq", "double_slit_fringe", "doppler_freq",
                 "beat_freq"):
        assert name in spec.SPECS, name
        registers, _ = spec.PROVENANCE[name]
        assert "applied context" in registers, name


def test_the_applied_family_narrates_and_never_half_writes():
    entry = spec.SPECS["conduction_rate"]
    args = [0.5, 2, 20, 0.1]
    applied = [structure for structure in grammar.structures(entry)
               if structure.given == "applied"]
    assert applied
    rendered = [grammar.render(entry, args, structure, "lab", 0)
                for structure in applied]
    for row in rendered:
        assert row is not None
        assert "In the setup," in row.text
        assert "{" not in row.text


@pytest.mark.skipif(not os.path.isdir(os.path.expanduser(
    "~/.capy/work/ndlessai-apchem/corpora")),
    reason="no local source corpus to check against")
def test_no_row_reproduces_seven_words_of_a_source():
    import gen_wording

    rows = gen_wording.all_rows(1)[0]
    assert gen_wording.source_guard(
        os.path.expanduser("~/.capy/work/ndlessai-apchem/corpora"), rows) == 0


def test_the_contrast_corpus_pairs_are_genuine_minimal_pairs():
    path = os.path.join(HERE, "fixtures", "paraphrase_contrast_r7.jsonl")
    with open(path) as handle:
        rows = [json.loads(line) for line in handle if line.strip()]
    assert rows
    pairs = {}
    for row in rows:
        assert row["pair"].split("/")[0] in RELATIONS, row["pair"]
        assert row["relation"] in row["pair"], row
        assert row["split"] == "train"
        assert row.get("missed_by"), row
        pairs.setdefault(row["pair"], []).append(row)
    for pair, side_rows in pairs.items():
        assert len({tuple(row["args"]) for row in side_rows}) == 1, pair
        assert {row["relation"] for row in side_rows} == set(pair.split("/")), pair


def test_generated_artifacts_are_current():
    result = subprocess.run([sys.executable, "gen_wording.py", "--check"],
                            cwd=HERE, capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
