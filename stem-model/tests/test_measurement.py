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


def test_the_floor_budget_counts_only_the_relations_it_judges():
    # Three rows is not enough to judge a relation, however bad it looks: the
    # denominator is the relations the floor is entitled to speak about, so a
    # catalogue that grows with thin rows does not dilute the share.
    per = {"thin_four": [4, 0], "thin_three": [3, 0], "fine": [10, 9]}
    thin, judged = report_relations.floor_count(per, 0.5, 4)
    assert (thin, judged) == (1, 2)


def test_a_relation_exactly_on_the_floor_is_not_below_it():
    per = {"half": [4, 2], "just_under": [4, 1]}
    thin, judged = report_relations.floor_count(per, 0.5, 4)
    assert thin == 1 and judged == 2


def test_the_share_is_a_ratchet_the_catalogue_growing_cannot_lower():
    # The same proportion of bad relations scores the same share whether the
    # catalogue has 141 judged relations or 500, which is what a fixed count
    # could not do: 25 of 141 and 89 of 500 are both 17.8%.
    small = {f"r{i}": [4, 0] for i in range(25)}
    small.update({f"g{i}": [10, 9] for i in range(116)})
    large = {f"r{i}": [4, 0] for i in range(89)}
    large.update({f"g{i}": [10, 9] for i in range(411)})
    assert report_relations.floor_count(small, 0.5, 4) == (25, 141)
    assert report_relations.floor_count(large, 0.5, 4) == (89, 500)
    assert 25 / 141 == pytest.approx(89 / 500, abs=0.002)
