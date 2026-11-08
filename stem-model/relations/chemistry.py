import sympy as sp

from .core import relation, _exact, dec, dec_exact


@relation("moles", "chemistry", ["mass", "molar_mass"], exact=False)
class Moles:
    @staticmethod
    def gen(rng):
        mm = rng.choice([18, 32, 44, 58.5, 98, 100])
        mass = rng.choice([5, 10, 11, 20, 22, 44, 50, 88])
        return [mass, mm], f"How many moles are in {mass} g of a substance with molar mass {mm} g/mol?"

    @staticmethod
    def solve(mass, molar_mass):
        return f"{dec(sp.Float(mass) / sp.Float(molar_mass))} mol"

@relation("dilution", "chemistry", ["c1", "v1", "v2"], exact=False)
class Dilution:
    @staticmethod
    def gen(rng):
        c1 = rng.choice([0.5, 1, 2, 4])
        v1 = rng.choice([10, 25, 50, 100])
        v2 = v1 + rng.choice([50, 100, 150, 200])
        q = (f"What is the molarity after diluting {v1} mL of {c1} M solution "
             f"to a final volume of {v2} mL?")
        return [c1, v1, v2], q

    @staticmethod
    def solve(c1, v1, v2):
        return f"{dec(sp.Float(c1) * sp.Float(v1) / sp.Float(v2))} M"

@relation("gas_p", "chemistry", ["n", "T", "V"], exact=False)
class GasP:
    @staticmethod
    def gen(rng):
        n = rng.choice([0.5, 1, 2, 3])
        T = rng.choice([273, 298, 300, 350])
        V = rng.choice([1, 2, 5, 10, 22.4])
        q = (f"Find the pressure of {n} mol of ideal gas at {T} K in {V} L. "
             f"Use R = 0.08206 L atm / (mol K).")
        return [n, T, V], q

    @staticmethod
    def solve(n, T, V):
        p = sp.Float(n) * sp.Float("0.08206") * sp.Float(T) / sp.Float(V)
        return f"{dec(p)} atm"

@relation("molarity", "chemistry", ["n", "V"], exact=False)
class Molarity:
    @staticmethod
    def gen(rng):
        n = rng.choice([0.1, 0.2, 0.25, 0.5, 1, 1.5, 2, 2.5, 3])
        V = rng.choice([0.5, 1, 2, 2.5, 4, 5, 10])
        return [n, V], (f"{n} mol of solute is dissolved in {V} L of "
                        f"solution. Find the molarity.")

    @staticmethod
    def solve(n, V):
        return f"{dec_exact(_exact(n) / _exact(V))} mol/L"

@relation("ph", "chemistry", ["H"], exact=False)
class Ph:
    @staticmethod
    def gen(rng):
        H = rng.choice([0.1, 0.01, 0.001, 0.0001, 0.05, 0.02, 0.005,
                        0.002, 0.0005, 0.0002])
        return [H], (f"A solution has a hydrogen ion concentration of "
                     f"{H} mol/L. Find its pH.")

    @staticmethod
    def solve(H):
        return dec(-sp.log(_exact(H), 10))

@relation("poh", "chemistry", ["OH"], exact=False)
class Poh:
    @staticmethod
    def gen(rng):
        OH = rng.choice([0.1, 0.01, 0.001, 0.0001, 0.05, 0.02, 0.005,
                         0.002, 0.0005, 0.0002])
        return [OH], (f"A solution has a hydroxide ion concentration of "
                      f"{OH} mol/L. Find its pOH.")

    @staticmethod
    def solve(OH):
        return dec(-sp.log(_exact(OH), 10))

@relation("ph_from_poh", "chemistry", ["poh"], exact=False)
class PhFromPoh:
    @staticmethod
    def gen(rng):
        poh = rng.choice([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13])
        return [poh], f"A solution has a pOH of {poh}. Find its pH."

    @staticmethod
    def solve(poh):
        return dec_exact(14 - _exact(poh))

@relation("percent_yield", "chemistry", ["actual", "theoretical"], exact=False)
class PercentYield:
    @staticmethod
    def gen(rng):
        theoretical = rng.choice([10, 20, 25, 40, 50, 80, 100, 120, 150])
        actual = rng.randint(1, theoretical)
        return [actual, theoretical], (f"A reaction should produce "
                                       f"{theoretical} g but yields "
                                       f"{actual} g. Find the percent yield.")

    @staticmethod
    def solve(actual, theoretical):
        return f"{dec_exact(sp.Rational(100 * actual, theoretical))}%"

@relation("boyles", "chemistry", ["P1", "V1", "V2"], exact=False)
class Boyles:
    @staticmethod
    def gen(rng):
        P1 = rng.choice([1, 2, 3, 4, 5, 6, 8, 10])
        V1 = rng.choice([1, 2, 3, 4, 5, 6, 8, 10])
        V2 = rng.choice([1, 2, 3, 4, 5, 6, 8, 10])
        return [P1, V1, V2], (f"A gas at {P1} atm occupies {V1} L. It is "
                              f"compressed to {V2} L at constant temperature. "
                              f"Find the new pressure.")

    @staticmethod
    def solve(P1, V1, V2):
        return f"{dec_exact(sp.Rational(P1 * V1, V2))} atm"

@relation("charles", "chemistry", ["V1", "T1", "T2"], exact=False)
class Charles:
    @staticmethod
    def gen(rng):
        V1 = rng.choice([1, 2, 3, 4, 5, 6, 8, 10])
        T1 = rng.choice([200, 250, 273, 300, 350, 400])
        T2 = rng.choice([200, 250, 273, 300, 350, 400, 450, 500])
        return [V1, T1, T2], (f"A gas occupies {V1} L at {T1} K. It is "
                              f"heated to {T2} K at constant pressure. "
                              f"Find the new volume.")

    @staticmethod
    def solve(V1, T1, T2):
        return f"{dec_exact(sp.Rational(V1 * T2, T1))} L"

@relation("combined_gas", "chemistry", ["P1", "V1", "T1", "P2", "T2"],
          exact=False)
class CombinedGas:
    @staticmethod
    def gen(rng):
        P1 = rng.choice([1, 2, 3, 4, 5])
        V1 = rng.choice([1, 2, 3, 4, 5, 6, 8, 10])
        T1 = rng.choice([200, 250, 273, 300, 350, 400])
        P2 = rng.choice([1, 2, 3, 4, 5])
        T2 = rng.choice([200, 250, 273, 300, 350, 400, 450, 500])
        return [P1, V1, T1, P2, T2], (
            f"A gas at {P1} atm occupies {V1} L at {T1} K. It changes to "
            f"{P2} atm at {T2} K. Find the new volume.")

    @staticmethod
    def solve(P1, V1, T1, P2, T2):
        return f"{dec_exact(sp.Rational(P1 * V1 * T2, T1 * P2))} L"

@relation("density", "chemistry", ["m", "V"], exact=False)
class Density:
    @staticmethod
    def gen(rng):
        m = rng.choice([10, 20, 25, 40, 50, 80, 100, 120, 150, 200])
        V = rng.choice([2, 4, 5, 8, 10, 20, 25, 40, 50])
        return [m, V], (f"A sample has a mass of {m} g and a volume of "
                        f"{V} mL. Find its density.")

    @staticmethod
    def solve(m, V):
        return f"{dec_exact(sp.Rational(m, V))} g/mL"

@relation("stoichiometry", "chemistry", ["moles_given", "ratio"], exact=False)
class Stoichiometry:
    @staticmethod
    def gen(rng):
        moles_given = rng.choice([0.5, 1, 1.5, 2, 2.5, 3, 4, 5, 6, 8])
        ratio = rng.choice([1, 2, 3, 4])
        return [moles_given, ratio], (
            f"{moles_given} mol of reactant reacts with a product ratio of "
            f"{ratio} to 1. Find the moles of product formed.")

    @staticmethod
    def solve(moles_given, ratio):
        return f"{dec_exact(_exact(moles_given) * ratio)} mol"
