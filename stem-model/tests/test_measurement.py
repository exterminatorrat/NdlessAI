import argparse
import collections

import pytest

import courses
import make_dataset
import report_relations


def _rows(n):
    return [{"id": i} for i in range(n)]


def test_the_hand_written_share_is_reached_not_rounded_down():
    picked = make_dataset.injected_rows(_rows(7), 50)
    assert len(picked) == 50


def test_a_larger_corpus_does_not_inject_fewer_rows():
    small = make_dataset.injected_rows(_rows(2618), 50000)
    large = make_dataset.injected_rows(_rows(2785), 50000)
    assert len(small) == len(large) == 50000


def test_the_remainder_is_spread_across_the_whole_corpus():
    picked = make_dataset.injected_rows(_rows(7), 50)
    counts = collections.Counter(row["id"] for row in picked)
    assert set(counts) == set(range(7))
    assert max(counts.values()) - min(counts.values()) <= 1


def test_a_corpus_larger_than_the_share_is_sampled_not_repeated():
    picked = make_dataset.injected_rows(_rows(400), 50)
    assert len(picked) == 50
    assert len({row["id"] for row in picked}) == 50


def test_a_zero_share_injects_nothing():
    assert make_dataset.injected_rows(_rows(3), 0) == []


def test_the_course_floors_score_the_set_they_were_recorded_on():
    args = argparse.Namespace(source="paraphrase", split="eval", corpus=None,
                              allow_drift=False)
    rows = report_relations.floor_rows(args)
    assert len(rows) == courses.FLOOR_EVAL_ROWS


def test_a_chain_row_keeps_its_own_call_and_answer():
    rows = make_dataset.paraphrase_rows(["fixtures/chain_cases.jsonl"])
    assert rows
    assert all(row["relation"] == "chain" for row in rows)
    assert rows[0]["call"].startswith("CALL ")
    assert " ; CALL " in rows[0]["call"]
    assert rows[0]["answer"]


def test_the_build_fails_when_a_chain_fixture_is_on_disk_but_not_in_the_glob():
    with pytest.raises(SystemExit):
        make_dataset.require_chains([{"relation": "ph"}],
                                    "fixtures/paraphrase_*.jsonl")


def test_the_build_accepts_a_glob_that_reaches_the_chain_fixture():
    make_dataset.require_chains([{"relation": "chain"}], "fixtures/*.jsonl")
