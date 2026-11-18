"""The composition graph is a derivation, so these tests pin what it derives.

Every case here is a fact a wrong graph would state differently: an edge that a
measured bank row needs, an edge the units allow and the dimensions refuse, and
a relation that can never open a chain because its answer is several numbers.
"""

import compose


def test_the_chain_the_bank_actually_asks_for():
    graph, _answers, _notes = compose.build_graph()
    assert compose.path(graph, "stoich_mass", "percent_yield", 2)
    assert not compose.path(graph, "stoich_mass", "percent_yield", 1)


def test_a_ph_chain_reaches_ph_from_poh_through_poh():
    graph, _answers, _notes = compose.build_graph()
    assert compose.path(graph, "mixing_concentration", "poh", 2)
    assert compose.path(graph, "poh", "ph_from_poh", 2)
    assert compose.path(graph, "mixing_concentration", "ph_from_poh", 3)


def test_a_multi_valued_answer_never_opens_a_chain():
    graph, answers, _notes = compose.build_graph()
    assert answers["quad_solve"][0] is False
    assert graph.get("quad_solve", []) == []


def test_the_ratio_slot_is_refused_by_its_dimension():
    """Both sides print `mol`, but the slot is a ratio: the report names it."""
    graph, _answers, notes = compose.build_graph()
    assert not compose.path(graph, "moles", "stoich_mass", 2)
    assert any("stoich_mass.ratio" in note for note in notes)


def test_the_two_spellings_of_molarity_are_one_unit():
    assert compose.canonical("M") == compose.canonical("mol/L")
    assert compose.canonical("percent") == compose.canonical("%")


def test_a_conversion_is_a_factor_and_not_an_equality():
    assert compose.FACTORS[("kJ", "J")] == 1000.0
    assert compose.convert("kJ", "J") == 1000.0
    assert compose.convert("K", "J") is None


def test_every_edge_carries_a_known_unit_or_a_dimension():
    graph, answers, _notes = compose.build_graph()
    for source, outs in graph.items():
        ok, unit = answers[source]
        assert ok, source
        for target, index, factor in outs:
            assert factor > 0, (source, target, index)
            if not unit:
                # Carried by dimensions rather than units, so both sides have
                # to declare one or the edge would be an assumption.
                assert source in compose.SPECS and target in compose.SPECS


def test_the_labelled_rows_still_agree_with_the_graph():
    """The number this page reports, pinned.

    `fixtures/chain_rows.txt` is a hand read, and the graph is a derivation, so
    they can drift apart in either direction: a relation that gains a slot, a
    label that stops matching. Both would change the count the page states.
    """
    import os
    from collections import Counter

    graph, _answers, _notes = compose.build_graph()
    rows_path = os.path.join(os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))), "fixtures", "chain_rows.txt")
    reads = Counter()
    paired = 0
    agree = 0
    with open(rows_path, encoding="utf-8") as handle:
        for line in handle:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            row_id, first, needed, calls, verdict, _note = line.split()
            reads[verdict] += 1
            if first == "-" or needed == "-":
                continue
            paired += 1
            graph_calls = None
            for n in range(1, compose.MAX_CALLS + 1):
                if compose.path(graph, first, needed, n):
                    graph_calls = n
                    break
            agree += graph_calls == int(calls)
    assert dict(reads) == {"blocked": 17, "chain-2": 3, "chain-1": 2,
                           "chain-3": 1}
    assert (agree, paired) == (6, 6)
