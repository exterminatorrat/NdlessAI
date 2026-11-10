import sympy as sp

from .core import relation, _exact, dec_exact

# ---------------------------------------------------------------------------
# Wave 3.5: electricity and magnetism.
#
# The unit gymnastics is deliberate and is stated in every question, because a
# calculator answer that is right in the wrong unit is worse than no answer:
#
#   - charge is in microcoulombs, so Coulomb's constant becomes 8.99e-3 N m^2
#     per microcoulomb squared and the result stays in newtons;
#   - capacitance is in microfarads, so Q = C V lands directly in microcoulombs;
#   - energy uses Q in microcoulombs and V in volts, giving microjoules.
#
# Every answer is an exact rational of those decimal arguments.
# ---------------------------------------------------------------------------


@relation("coulomb_force", "physics", ["q1", "q2", "r"], exact=False)
class CoulombForce:
    @staticmethod
    def gen(rng):
        q1 = rng.choice([1, 2, 3, 4, 5, 8])
        q2 = rng.choice([1, 2, 3, 4, 6, 10])
        r = rng.choice([0.5, 1, 2, 3, 4])
        return [q1, q2, r], (
            f"Two charges of {q1} uC and {q2} uC are {r} m apart. Find the "
            f"force between them, with k = 8.99e9 N m^2/C^2.")

    @staticmethod
    def solve(q1, q2, r):
        k = _exact("0.00899")		# k q^2 with both q in microcoulombs
        return f"{dec_exact(k * _exact(q1) * _exact(q2) / _exact(r) ** 2)} N"


@relation("electric_field_point", "physics", ["q", "r"], exact=False)
class ElectricFieldPoint:
    @staticmethod
    def gen(rng):
        q = rng.choice([1, 2, 3, 4, 5, 8, 10])
        r = rng.choice([1, 2, 3, 4, 5])
        return [q, r], (f"Find the electric field {r} m from a point charge "
                        f"of {q} uC, with k = 8.99e9 N m^2/C^2.")

    @staticmethod
    def solve(q, r):
        k = _exact("8990")		# k q / r^2 with q in microcoulombs
        return f"{dec_exact(k * _exact(q) / _exact(r) ** 2)} N/C"


@relation("capacitor_charge", "physics", ["C", "V"], exact=False)
class CapacitorCharge:
    @staticmethod
    def gen(rng):
        C = rng.choice([2, 5, 10, 20, 47, 100])
        V = rng.choice([3, 5, 9, 12, 24])
        return [C, V], (f"A {C} uF capacitor is connected to {V} V. Find the "
                        f"charge stored.")

    @staticmethod
    def solve(C, V):
        return f"{dec_exact(_exact(C) * _exact(V))} uC"


@relation("capacitor_energy", "physics", ["Q", "V"], exact=False)
class CapacitorEnergy:
    @staticmethod
    def gen(rng):
        Q = rng.choice([10, 20, 40, 60, 120, 200])
        V = rng.choice([3, 5, 9, 12, 24])
        return [Q, V], (f"A capacitor holds {Q} uC at {V} V. Find the energy "
                        f"stored.")

    @staticmethod
    def solve(Q, V):
        return f"{dec_exact(_exact(Q) * _exact(V) / 2)} uJ"


@relation("parallel_resistance", "physics", ["r1", "r2"], exact=False)
class ParallelResistance:
    @staticmethod
    def gen(rng):
        r1 = rng.choice([2, 3, 4, 6, 8, 12, 20])
        r2 = rng.choice([2, 3, 4, 6, 8, 12, 20])
        return [r1, r2], (f"Two resistors of {r1} ohm and {r2} ohm are "
                          f"connected in parallel. Find the equivalent "
                          f"resistance.")

    @staticmethod
    def solve(r1, r2):
        return f"{dec_exact(_exact(r1) * _exact(r2) / (_exact(r1) + _exact(r2)))} ohm"


@relation("motional_emf", "physics", ["B", "L", "v"], exact=False)
class MotionalEmf:
    @staticmethod
    def gen(rng):
        B = rng.choice([0.2, 0.5, 1, 1.5, 2])
        L = rng.choice([0.5, 1, 2, 3])
        v = rng.choice([2, 4, 5, 10])
        return [B, L, v], (f"A rod {L} m long moves at {v} m/s through a "
                           f"magnetic field of {B} T. Find the motional emf.")

    @staticmethod
    def solve(B, L, v):
        return f"{dec_exact(_exact(B) * _exact(L) * _exact(v))} V"
