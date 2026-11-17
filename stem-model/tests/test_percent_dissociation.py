"""percent_dissociation against an independent calculation.

The relation's C solver and its Python solver both take the square root on an
integer grid, so comparing them to each other proves only that the grid is
implemented twice the same way. These two tests compute the answer a different
way: `math.sqrt` on a double, and the exact quadratic, which is the value the
small-x approximation is an approximation *of*.
"""

import math
import random

import pytest

from relations import RELATIONS


REL = RELATIONS["percent_dissociation"]

# The pools the generator draws from, exercised exhaustively rather than by
# sampling, so a value that lands badly on the grid cannot hide.
C0 = [0.05, 0.1, 0.2, 0.5, 1, 2]
KA = [0.0001, 0.00018, 0.00029, 0.0004, 0.00063]


def printed_percent(c0, ka):
    return float(REL.solve(c0, ka).rstrip("%"))


@pytest.mark.parametrize("c0", C0)
@pytest.mark.parametrize("ka", KA)
def test_matches_double_precision_square_root(c0, ka):
    if ka / c0 > 0.0025:
        pytest.skip("outside the regime the relation declares")
    want = 100.0 * math.sqrt(ka / c0)
    got = printed_percent(c0, ka)
    # Four printed figures: the last place is worth 5e-4 relative, and the
    # double-precision root is good to far more than that.
    assert abs(got - want) <= 5e-4 * want, (c0, ka, got, want)


@pytest.mark.parametrize("seed", range(40))
def test_small_x_claim_holds_for_every_generated_row(seed):
    """The solver is the approximation, so the approximation has to be decent.

    Solving Ka = x^2/(c0 - x) for x gives the exact answer; where the generator
    is allowed to go, it has to agree with the approximation to better than one
    percent, or the relation is answering a question it did not ask.
    """

    c0, ka = REL.gen(random.Random(seed))[0]
    approx = printed_percent(c0, ka)
    exact = 100.0 * (-ka + math.sqrt(ka * ka + 4 * ka * c0)) / (2 * c0)
    assert abs(approx - exact) / exact < 0.01, (c0, ka, approx, exact)
