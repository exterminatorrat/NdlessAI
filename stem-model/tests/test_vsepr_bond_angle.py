"""vsepr_bond_angle against geometry and against measured values.

A table cannot be verified by computing it, so this file does the next best
thing and separates the two kinds of row it holds:

- four of the six angles follow from geometry, and are recomputed here from an
  independent construction (360/3, acos(-1/3), three perpendicular axes, two
  opposite directions), never from the table's own numbers;
- two are measured values for ammonia and water. They are written out again
  here from the reference figures, which is a citation and not a proof, and
  this file says so rather than implying the table checks itself.

What is *not* covered by a gate: the C solver's refusal of a shape that is not
in the table. The parity harness only draws pairs the generator can produce, and
the generator only produces tabulated ones. The Python refusal is checked below.
"""

import math

import pytest

from relations import RELATIONS


REL = RELATIONS["vsepr_bond_angle"]

DERIVED = {
    (2, 0): 180.0,                      # two domains, opposite directions
    (3, 0): 360.0 / 3,                  # three domains sharing a plane
    (4, 0): math.degrees(math.acos(-1.0 / 3.0)),   # tetrahedron
    (6, 0): 90.0,                       # three mutually perpendicular axes
}

# Measured, not derived: the bond angles of ammonia and water are empirical.
MEASURED = {
    (3, 1): 107.0,
    (2, 2): 104.5,
}


@pytest.mark.parametrize("pair,want", sorted(DERIVED.items()))
def test_derived_angles(pair, want):
    got = float(REL.solve(*pair))
    # The table stores the conventional 109.5 for the tetrahedron, where the
    # geometry gives 109.4712: a half-degree of rounding, not an error.
    assert abs(got - want) <= 0.05, (pair, got, want)


@pytest.mark.parametrize("pair,want", sorted(MEASURED.items()))
def test_measured_angles(pair, want):
    assert float(REL.solve(*pair)) == pytest.approx(want)


def test_every_other_shape_is_refused():
    for domains in range(1, 8):
        for lone in range(0, domains + 1):
            if (domains, lone) in DERIVED or (domains, lone) in MEASURED:
                continue
            if lone > domains:
                continue
            with pytest.raises(ValueError):
                REL.solve(domains, lone)


def test_generator_only_draws_tabulated_shapes():
    import random

    for seed in range(60):
        args, _q = REL.gen(random.Random(seed))
        assert tuple(args) in DERIVED or tuple(args) in MEASURED
