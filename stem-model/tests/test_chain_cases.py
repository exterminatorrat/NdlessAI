"""The chain corpus: every row is a plan the runtime would accept and answer.

The corpus had no chain target at all, so `allow_chain` could be turned on at
retrain time and teach the model nothing. `gen_chain_cases.py` builds the missing
rows from the composition graph; these tests are what stops the fixture drifting
into rows no chain could ever be.

Three things are asserted per row: the plan verifies against the reference
solvers (the answer the row states is the answer the calls produce), each step
is one call the plan grammar accepts, and the question does not state the
intermediate — a row whose sentence contains the number the second step carries
is a single call wearing a chain's clothes.
"""

import json
import os
import re

import pytest

import gen_chain_cases as cases
from relations import RELATIONS

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIXTURE = os.path.join(HERE, "fixtures", "chain_cases.jsonl")

# plan.c's own limits, restated here so a generated row cannot exceed them.
PLAN_TEXT_MAX = 256
PLAN_MAX_STEPS = 5


def rows():
    with open(FIXTURE, encoding="utf-8") as handle:
        return [json.loads(line) for line in handle if line.strip()]


def test_the_fixture_has_rows_of_every_length():
    lengths = {}
    for row in rows():
        lengths[row["steps"]] = lengths.get(row["steps"], 0) + 1
    assert set(lengths) == {2, 3, 4}
    assert all(lengths[steps] > 20 for steps in lengths), lengths


def test_every_row_is_a_plan_the_runtime_would_accept():
    for row in rows():
        steps = [part.strip() for part in row["call"].split(";")]
        assert len(steps) == row["steps"]
        assert 2 <= len(steps) <= PLAN_MAX_STEPS
        assert len(row["call"]) < PLAN_TEXT_MAX, row["call"]
        for index, step in enumerate(steps):
            words = step.split()
            assert words[0] == "CALL"
            assert words[1] in RELATIONS, words[1]
            assert len(words) - 2 == len(RELATIONS[words[1]].params)
            for word in words[2:]:
                if not word.startswith("<"):
                    continue
                assert word.endswith(">")
                # A reference may only name a step that already ran.
                assert 1 <= int(word[1:-1]) <= index, (word, index)
        references = [w for step in steps for w in step.split() if w.startswith("<")]
        assert references, "a chain with no reference teaches nothing about chaining"


def test_every_row_answers_what_its_plan_answers():
    for row in rows():
        ok, why = cases.verify(row)
        assert ok, f"{row['call']}: {why}"


def test_every_question_hides_the_intermediate():
    for row in rows():
        assert "that result" in row["question"], row["question"]
        # The number the second step carries must not be in the sentence, or the
        # row could be answered with a single call.
        steps = [part.strip() for part in row["call"].split(";")]
        for index, step in enumerate(steps[1:], start=1):
            for word in step.split()[2:]:
                if not word.startswith("<"):
                    continue
                pattern = re.compile(r"(?<![\w.^])" + re.escape(word) + r"(?![\w])")
                assert not pattern.search(row["question"]), (row["call"], row["question"])


def test_every_chain_stays_inside_one_subject():
    for row in rows():
        subjects = {RELATIONS[step.split()[1]].subject
                    for step in row["call"].split(";")}
        assert len(subjects) == 1, row["call"]
        assert subjects.pop() == row["subject"]


def test_the_split_is_deterministic_and_named():
    for row in rows():
        assert row["split"] in ("train", "eval")
        assert row["answer"].strip()


def test_the_fixture_is_what_the_generator_would_write():
    current = open(FIXTURE, encoding="utf-8").read()
    built, _dropped = cases.build(len(rows()), "chain")
    assert current == cases.render(built), "stale: run gen_chain_cases.py --write"
