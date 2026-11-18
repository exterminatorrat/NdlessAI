import math

import sympy as sp

from .core import relation, _exact, dec, dec_exact


@relation("moles", "chemistry", ["mass", "molar_mass"], exact=False,
          step=dict(line="n = m/M", expr="@1 / @2"))
class Moles:
    @staticmethod
    def gen(rng):
        mm = rng.choice([18, 32, 44, 58.5, 98, 100])
        mass = rng.choice([5, 10, 11, 20, 22, 44, 50, 88])
        return [mass, mm], f"How many moles are in {mass} g of a substance with molar mass {mm} g/mol?"

    @staticmethod
    def solve(mass, molar_mass):
        return f"{dec(sp.Float(mass) / sp.Float(molar_mass))} mol"

@relation("dilution", "chemistry", ["c1", "v1", "v2"], exact=False,
          step=dict(line="c2 = c1 * v1 / v2", expr="@1 * @2 / @3"))
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

@relation("gas_p", "chemistry", ["n", "T", "V"], exact=False,
          step=dict(line="P = n*R*T/V, R = 0.08206", expr="@1*0.08206*@2/@3"))
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

@relation("molarity", "chemistry", ["n", "V"], exact=False,
          step=dict(line="M = n/V", expr="@1 / @2"))
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

@relation("ph", "chemistry", ["H"], exact=False,
          step=dict(line="pH = -log10[H+]", expr="-log10(@1)"))
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

@relation("poh", "chemistry", ["OH"], exact=False,
          step=dict(line="pOH = -log10[OH-]", expr="-log10(@1)"))
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

@relation("ph_from_poh", "chemistry", ["poh"], exact=False,
          step=dict(line="pH = 14 - pOH", expr="14 - @1"))
class PhFromPoh:
    @staticmethod
    def gen(rng):
        poh = rng.choice([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13])
        return [poh], f"A solution has a pOH of {poh}. Find its pH."

    @staticmethod
    def solve(poh):
        return dec_exact(14 - _exact(poh))

@relation("percent_yield", "chemistry", ["actual", "theoretical"], exact=False,
          step=dict(line="yield% = actual/theoretical*100", expr="@1/@2*100"))
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

@relation("boyles", "chemistry", ["P1", "V1", "V2"], exact=False,
          step=dict(line="P2 = P1*V1/V2", expr="@1*@2/@3"))
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

@relation("charles", "chemistry", ["V1", "T1", "T2"], exact=False,
          step=dict(line="V2 = V1*T2/T1", expr="@1*@3/@2"))
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
          exact=False,
          step=dict(line="P2 = P1*V1*T2/(T1*V2)",
                    expr="@1*@2*@5/(@3*@4)"))
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

@relation("density", "chemistry", ["m", "V"], exact=False,
          step=dict(line="d = m/V", expr="@1/@2"))
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

@relation("stoichiometry", "chemistry", ["moles_given", "ratio"], exact=False,
          step=dict(line="product = moles*ratio", expr="@1*@2"))
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


def _num(value):
    """A derived quantity as the int or decimal token the question shows."""

    r = _exact(value)
    return int(r) if r.q == 1 else float(r)

@relation("dilution_v1", "chemistry", ["m1", "m2", "v2"], exact=False,
          step=dict(line="V1 = M2*V2/M1", expr="@2*@3/@1"))
class DilutionV1:
    @staticmethod
    def gen(rng):
        m1 = rng.choice([0.5, 1, 2, 4, 5])
        m2 = rng.choice([0.1, 0.2, 0.25, 0.5, 1])
        v2 = rng.choice([50, 100, 200, 250, 500])
        q = (f"What volume of {m1} M stock solution is needed to prepare "
             f"{v2} mL of {m2} M solution?")
        return [m1, m2, v2], q

    @staticmethod
    def solve(m1, m2, v2):
        return f"{dec_exact(_exact(m2) * _exact(v2) / _exact(m1))} mL"

@relation("molality", "chemistry", ["n", "kg"], exact=False,
          step=dict(line="m = n / kg", expr="@1 / @2"))
class Molality:
    @staticmethod
    def gen(rng):
        n = rng.choice([0.1, 0.2, 0.25, 0.5, 0.75, 1, 1.5, 2, 2.5, 3])
        kg = rng.choice([0.25, 0.5, 1, 2, 2.5, 4, 5])
        return [n, kg], (f"{n} mol of solute is dissolved in {kg} kg of "
                         f"solvent. Find the molality.")

    @staticmethod
    def solve(n, kg):
        return f"{dec_exact(_exact(n) / _exact(kg))} mol/kg"

@relation("h_from_ph", "chemistry", ["ph"], exact=False,
          step=dict(line="[H+] = 10^(-pH)", expr="10^(-@1)"))
class HFromPh:
    @staticmethod
    def gen(rng):
        ph = rng.randint(1, 13)
        return [ph], (f"A solution has a pH of {ph}. Find its hydrogen ion "
                      f"concentration.")

    @staticmethod
    def solve(ph):
        # The device path refuses a non-integer pH rather than answering it, so
        # the reference must refuse too. int(ph) would turn pH 2.5 into 10^-2
        # and 0.01 mol/L where the answer is 0.00316, and a corpus row written
        # from that would teach the model a wrong answer.
        if sp.Rational(ph).q != 1:
            raise ValueError("h_from_ph takes an integer pH")
        return f"{dec_exact(sp.Rational(10) ** -int(ph))} mol/L"

@relation("buffer_ph", "chemistry", ["pka", "base", "acid"], exact=False,
          step=dict(line="pH = pKa + log10(base/acid)", expr="@1 + log10(@2/@3)"))
class BufferPh:
    @staticmethod
    def gen(rng):
        pka = rng.choice([3.75, 4.76, 5, 7.2, 9.25, 10.5])
        base = rng.choice([0.1, 0.2, 0.5, 1])
        acid = rng.choice([0.1, 0.2, 0.5, 1])
        return [pka, base, acid], (
            f"A buffer contains {base} M conjugate base and {acid} M weak "
            f"acid with pKa {pka}. Find the pH.")

    @staticmethod
    def solve(pka, base, acid):
        return dec(_exact(pka) + sp.log(_exact(base) / _exact(acid), 10))

@relation("calorimetry_mass", "chemistry", ["q", "c", "dT"], exact=False,
          step=dict(line="m = q/(c*dT)", expr="@1/(@2*@3)"))
class CalorimetryMass:
    @staticmethod
    def gen(rng):
        m = rng.choice([2, 3, 4, 5, 8, 10, 15, 20, 25, 50])
        c = rng.choice([0.45, 0.9, 1, 2.1, 2.4, 4.18, 4.2])
        dT = rng.choice([5, 10, 15, 20, 25, 30, 40, 50])
        q = _num(_exact(m) * _exact(c) * dT)
        return [q, c, dT], (f"{q} J of heat raises a sample of specific heat "
                            f"{c} J/(g C) by {dT} degrees. Find its mass.")

    @staticmethod
    def solve(q, c, dT):
        return f"{dec_exact(_exact(q) / (_exact(c) * _exact(dT)))} g"

@relation("calorimetry_c", "chemistry", ["q", "m", "dT"], exact=False,
          step=dict(line="c = q/(m*dT)", expr="@1/(@2*@3)"))
class CalorimetryC:
    @staticmethod
    def gen(rng):
        m = rng.choice([5, 10, 20, 25, 40, 50, 100, 200])
        c = rng.choice([0.13, 0.24, 0.385, 0.45, 0.9, 2.1, 4.18])
        dT = rng.choice([5, 10, 15, 20, 25, 30, 40, 50])
        q = _num(_exact(m) * _exact(c) * dT)
        return [q, m, dT], (f"{m} g of material absorbs {q} J as it warms by "
                            f"{dT} degrees. Find its specific heat.")

    @staticmethod
    def solve(q, m, dT):
        return f"{dec_exact(_exact(q) / (_exact(m) * _exact(dT)))} J/(g C)"

@relation("latent_heat", "chemistry", ["m", "L"], exact=False,
          step=dict(line="Q = m * L", expr="@1 * @2"))
class LatentHeat:
    @staticmethod
    def gen(rng):
        m = rng.choice([5, 10, 20, 25, 40, 50, 100, 200])
        L = rng.choice([334, 2260])
        return [m, L], (f"{m} g of a substance with latent heat {L} J/g "
                        f"changes phase completely. Find the energy required.")

    @staticmethod
    def solve(m, L):
        return f"{dec_exact(_exact(m) * _exact(L))} J"

@relation("first_law", "chemistry", ["q", "w"], exact=False,
          step=dict(line="dU = q + w", expr="@1 + @2"))
class FirstLaw:
    @staticmethod
    def gen(rng):
        q = rng.choice([-200, -100, 0, 100, 150, 200, 250, 300, 500])
        w = rng.choice([-100, -50, 0, 50, 100, 150, 200])
        return [q, w], (f"The heat added to a system is {q} J and the work "
                        f"done on it is {w} J. Find the change in internal "
                        f"energy.")

    @staticmethod
    def solve(q, w):
        return f"{dec_exact(_exact(q) + _exact(w))} J"

@relation("gibbs", "chemistry", ["dH", "T", "dS"], exact=False,
          step=dict(line="dG = dH - T*dS/1000", expr="@1 - @2*@3/1000"))
class Gibbs:
    @staticmethod
    def gen(rng):
        dH = rng.choice([-600, -500, -300, -200, -100, 0, 100, 200, 400])
        T = rng.choice([200, 250, 298, 300, 350, 400, 500])
        dS = rng.choice([-400, -300, -250, -200, -150, -100, 50, 100, 150,
                         200, 250, 300, 400])
        return [dH, T, dS], (f"A reaction has an enthalpy change of {dH} kJ "
                             f"and an entropy change of {dS} J/K at {T} K. "
                             f"Find the change in Gibbs free energy.")

    @staticmethod
    def solve(dH, T, dS):
        return f"{dec_exact(_exact(dH) - _exact(dS) * _exact(T) / 1000)} kJ"

@relation("carnot", "chemistry", ["t_hot", "t_cold"], exact=False,
          step=dict(line="eff = (Th - Tc)/Th", expr="(@1 - @2) / @1"))
class Carnot:
    @staticmethod
    def gen(rng):
        t_cold = rng.choice([200, 250, 300, 350, 400, 500])
        t_hot = t_cold + rng.choice([100, 150, 200, 300, 400, 500])
        return [t_hot, t_cold], (f"A Carnot engine operates between {t_hot} K "
                                 f"and {t_cold} K. Find its efficiency as a decimal.")

    @staticmethod
    def solve(t_hot, t_cold):
        # A fraction, like every other efficiency in the catalogue. This relation
        # and `carnot_efficiency` are one quantity under two names, and one
        # quantity has one format (ELV-77).
        return dec_exact(sp.Rational(t_hot - t_cold, t_hot))

# ---------------------------------------------------------------------------
# Wave 3.3: the parts of AP Chemistry the catalogue had no relation for.
#
# Every answer here is an exact rational of the decimal arguments, rendered
# with dec_exact. That is deliberate: these are the relations where a student
# checks the arithmetic, so "7.1 g" instead of "7.142 g" is the kind of thing
# that costs a mark, and rounding the exact rational is what keeps the C
# renderer and the catalogue byte-identical.
# ---------------------------------------------------------------------------


@relation("mass_percent", "chemistry", ["solute", "solution"], exact=False,
          step=dict(line="mass% = solute/solution * 100", expr="@1 / @2 * 100"))
class MassPercent:
    @staticmethod
    def gen(rng):
        solution = rng.choice([100, 200, 250, 400, 500])
        solute = rng.choice([5, 10, 12.5, 20, 25, 40, 50])
        return [solute, solution], (
            f"A solution is made from {solute} g of solute and "
            f"{solution} g of solution. Find the mass percent of the solute.")

    @staticmethod
    def solve(solute, solution):
        return f"{dec_exact(_exact(solute) / _exact(solution) * 100)}%"


@relation("stoich_mass", "chemistry",
          ["mass", "mm_given", "mm_product", "ratio"], exact=False,
          step=dict(line="mass = mass_given*ratio*mm_product/mm_given",
                    expr="@1*@4*@3/@2"))
class StoichMass:
    @staticmethod
    def gen(rng):
        mass = rng.choice([9, 10, 18, 20, 25, 36, 50, 90])
        mm_given = rng.choice([18, 32, 44, 58.5, 100])
        mm_product = rng.choice([18, 44, 58.5, 80, 100])
        ratio = rng.choice([1, 2, 3])
        return [mass, mm_given, mm_product, ratio], (
            f"{mass} g of a reactant with molar mass {mm_given} g/mol reacts. "
            f"Each mole of it gives {ratio} mol of product, whose molar mass is "
            f"{mm_product} g/mol. Find the mass of product.")

    @staticmethod
    def solve(mass, mm_given, mm_product, ratio):
        moles = _exact(mass) / _exact(mm_given)
        return f"{dec_exact(moles * _exact(ratio) * _exact(mm_product))} g"


@relation("mole_fraction", "chemistry", ["na", "nb"], exact=False,
          step=dict(line="xA = nA/(nA + nB)", expr="@1 / (@1 + @2)"))
class MoleFraction:
    @staticmethod
    def gen(rng):
        na = rng.choice([1, 1.5, 2, 3, 4])
        nb = rng.choice([1, 2, 3, 5, 6])
        return [na, nb], (f"A mixture holds {na} mol of gas A and {nb} mol of "
                          f"gas B. Find the mole fraction of A.")

    @staticmethod
    def solve(na, nb):
        return dec_exact(_exact(na) / (_exact(na) + _exact(nb)))


@relation("kc_from_conc", "chemistry", ["product", "reactant", "coefficient"],
          exact=False,
          step=dict(line="Kc = product^coef / reactant", expr="@1^@3 / @2"))
class KcFromConc:
    @staticmethod
    def gen(rng):
        product = rng.choice([0.1, 0.2, 0.5, 1, 1.5])
        reactant = rng.choice([0.1, 0.25, 0.5, 1, 2])
        coefficient = rng.choice([1, 2])
        return [product, reactant, coefficient], (
            f"At equilibrium a reaction mixture has {product} M of product and "
            f"{reactant} M of reactant. The product has coefficient "
            f"{coefficient} and the reactant coefficient 1. Find Kc.")

    @staticmethod
    def solve(product, reactant, coefficient):
        return dec_exact(_exact(product) ** coefficient / _exact(reactant))


@relation("avg_rate", "chemistry", ["change", "time"], exact=False,
          step=dict(line="rate = change/time", expr="@1 / @2"))
class AvgRate:
    @staticmethod
    def gen(rng):
        change = rng.choice([0.1, 0.2, 0.4, 0.5, 1])
        time = rng.choice([10, 20, 25, 50, 100])
        return [change, time], (
            f"The concentration of a reactant falls by {change} M over "
            f"{time} s. Find the average rate of the reaction.")

    @staticmethod
    def solve(change, time):
        return f"{dec_exact(_exact(change) / _exact(time))} M/s"


@relation("heat_reaction", "chemistry", ["moles", "enthalpy"], exact=False,
          step=dict(line="q = n * dH", expr="@1 * @2"))
class HeatReaction:
    @staticmethod
    def gen(rng):
        moles = rng.choice([0.5, 1, 2, 2.5, 3])
        enthalpy = rng.choice([100, 250, 285.8, 393.5, 500, 890.3])
        return [moles, enthalpy], (
            f"A reaction releases {enthalpy} kJ per mole. Find the heat "
            f"released when {moles} mol reacts.")

    @staticmethod
    def solve(moles, enthalpy):
        return f"{dec_exact(_exact(moles) * _exact(enthalpy))} kJ"


@relation("limiting_moles", "chemistry", ["mass_a", "mm_a", "mass_b", "mm_b"],
          exact=False)
class LimitingMoles:
    @staticmethod
    def gen(rng):
        mm_a = rng.choice([2, 4, 16, 18, 32, 40, 44])
        mm_b = rng.choice([2, 16, 18, 32, 40, 44, 58])
        n_a = rng.choice([1, 2, 3, 4, 5, 6])
        n_b = rng.choice([1, 2, 3, 4, 5, 6])
        while n_b == n_a:
            n_b = rng.choice([1, 2, 3, 4, 5, 6])
        mass_a = mm_a * n_a
        mass_b = mm_b * n_b
        return [mass_a, mm_a, mass_b, mm_b], (
            f"{mass_a} g of a reagent with molar mass {mm_a} g/mol reacts with "
            f"{mass_b} g of one with molar mass {mm_b} g/mol. Find the smaller "
            f"number of moles.")

    @staticmethod
    def solve(mass_a, mm_a, mass_b, mm_b):
        return dec_exact(min(sp.Rational(mass_a, mm_a), sp.Rational(mass_b, mm_b)))


# ---------------------------------------------------------------------------
# AP Chemistry batch A. The bank's audit named these as relations it could not
# reach; each is a single exact step over the numbers the sentence states, so
# each answer is an exact rational of those decimals and the rounding is
# dec_exact's.
#
# Argument order is the order the sentence reads them, not the order the
# formula is written in on paper. `average_rate_of_change` failed every held-out
# row because its call had to be reordered out of the sentence, so each of these
# is checked against its own phrasings in surface/chemistry.py before it lands.
# ---------------------------------------------------------------------------


@relation("partial_pressure", "chemistry", ["n_i", "n_total", "P_total"],
          exact=False,
          step=dict(line="P_i = n_i * P_total / n_total",
                    expr="@1 * @3 / @2"))
class PartialPressure:
    @staticmethod
    def gen(rng):
        n_total = rng.choice([0.5, 1, 1.5, 2, 2.5, 3, 4, 5])
        n_i = rng.choice([0.1, 0.2, 0.25, 0.5, 1, 1.5, 2])
        while n_i >= n_total:
            n_i = rng.choice([0.1, 0.2, 0.25, 0.5, 1, 1.5, 2])
        P_total = rng.choice([1, 2, 3, 4, 5, 6, 8, 10])
        return [n_i, n_total, P_total], (
            f"A gas mixture contains {n_i} mol of one component out of "
            f"{n_total} mol of gas in total. The total pressure is "
            f"{P_total} atm. Find the partial pressure of that component.")

    @staticmethod
    def solve(n_i, n_total, P_total):
        p_i = _exact(n_i) * _exact(P_total) / _exact(n_total)
        return f"{dec_exact(p_i)} atm"


@relation("cell_potential", "chemistry", ["e_cathode", "e_anode"],
          exact=False,
          step=dict(line="E_cell = E_cathode - E_anode", expr="@1 - @2"))
class CellPotential:
    @staticmethod
    def gen(rng):
        e_cathode = rng.choice([0.13, 0.34, 0.4, 0.52, 0.8, 1.36, 1.51, 2.87])
        e_anode = rng.choice([-0.76, -0.44, -0.28, -0.14, 0, 0.13, 0.34, 0.8])
        return [e_cathode, e_anode], (
            f"A voltaic cell has a cathode whose standard reduction potential "
            f"is {e_cathode} V and an anode whose standard reduction potential "
            f"is {e_anode} V. Find the standard cell potential.")

    @staticmethod
    def solve(e_cathode, e_anode):
        return f"{dec_exact(_exact(e_cathode) - _exact(e_anode))} V"


@relation("ksp_from_solubility", "chemistry",
          ["s", "cation_count", "anion_count"], exact=False,
          step=dict(line="Ksp = (n*s)^n * (m*s)^m",
                    expr="(@1*@2)^@2 * (@1*@3)^@3"))
class KspFromSolubility:
    @staticmethod
    def _ksp(s, n, m):
        x = _exact(s)
        return (x * n) ** n * (x * m) ** m

    @staticmethod
    def gen(rng):
        # A salt whose arithmetic leaves the renderer: 1/(1000^5) is fine,
        # 1/(10000^6) is not, so the combination is drawn until it renders.
        while True:
            s = rng.choice([0.001, 0.002, 0.005, 0.01, 0.02, 0.05])
            n = rng.choice([1, 2, 3])
            m = rng.choice([1, 2, 3])
            ksp = KspFromSolubility._ksp(s, n, m)
            if ksp.p <= 9 * 10 ** 15 and ksp.q <= 10 ** 17:
                break
        return [s, n, m], (
            f"A sparingly soluble salt dissolves to give {s} mol per litre of "
            f"dissolved formula units. Each formula unit supplies {n} cations "
            f"and {m} anions. Find the solubility product constant Ksp.")

    @staticmethod
    def solve(s, n, m):
        return dec_exact(KspFromSolubility._ksp(s, n, m))


@relation("beer_lambert", "chemistry", ["A", "epsilon", "l"], exact=False,
          step=dict(line="c = A / (epsilon * l)", expr="@1 / (@2 * @3)"))
class BeerLambert:
    @staticmethod
    def gen(rng):
        A = rng.choice([0.15, 0.25, 0.3, 0.45, 0.6, 0.75, 0.9])
        epsilon = rng.choice([150, 1200, 1500, 2500, 4000])
        l = rng.choice([1, 2, 5])
        return [A, epsilon, l], (
            f"A solution has an absorbance of {A} at a wavelength where its "
            f"molar absorptivity is {epsilon} L/(mol cm), measured through a "
            f"path length of {l} cm. Find its concentration.")

    @staticmethod
    def solve(A, epsilon, l):
        return f"{dec_exact(_exact(A) / (_exact(epsilon) * _exact(l)))} mol/L"


@relation("atoms_from_mass", "chemistry", ["mass", "molar_mass"], exact=False,
          step=dict(line="N = (m/M) * 6.022e23", expr="@1 / @2 * 6.022 * 10^23"))
class AtomsFromMass:
    @staticmethod
    def _normalised(mass, molar_mass):
        """N as a mantissa in [1, 10) and the exponent it belongs to.

        The renderer takes a numerator up to 9e15 and an atom count is around
        1e22, so the answer cannot be one rational handed to dec4_render. It is
        the same number written in scientific notation, with the shifting done
        on the exact rational before any rounding, so both sides print the same
        four figures.
        """

        r = _exact(mass) / _exact(molar_mass) * sp.Rational(6022, 1000)
        e = 0
        while r >= 10:
            r /= 10
            e += 1
        while r < 1:
            r *= 10
            e -= 1
        return r, 23 + e

    @staticmethod
    def gen(rng):
        mass = rng.choice([1, 2, 5, 10, 18, 25, 44, 58, 100, 180, 250])
        mm = rng.choice([18, 44, 58.5, 98, 142, 180, 342])
        return [mass, mm], (
            f"A sample of a substance has a mass of {mass} g and a molar mass "
            f"of {mm} g/mol. Find the number of atoms in the sample, using "
            f"6.022 x 10^23 for Avogadro's number.")

    @staticmethod
    def solve(mass, molar_mass):
        mant, e = AtomsFromMass._normalised(mass, molar_mass)
        return f"{dec_exact(mant)}e+{e:02d} atoms"


@relation("wavelength_from_frequency", "chemistry", ["frequency_thz"],
          exact=False,
          step=dict(line="lambda = c / f, c = 2.998e8 m/s",
                    expr="2.998 * 10^8 / (@1 * 10^12)"))
class WavelengthFromFrequency:
    @staticmethod
    def gen(rng):
        f = rng.choice([150, 200, 300, 400, 500, 600, 750, 900, 1000])
        return [f], (
            f"Electromagnetic radiation has a frequency of {f} THz. Find its "
            f"wavelength in metres, using 2.998 x 10^8 m/s for the speed of "
            f"light.")

    @staticmethod
    def solve(frequency_thz):
        # c = 2.998e8 m/s and the argument is in THz, so the constant that
        # survives is c * 1e-12 = 2.998e-4 m per (1/THz).
        lam = sp.Rational(2998, 10) * 10 ** -6 / _exact(frequency_thz)
        return f"{dec_exact(lam)} m"


# ---------------------------------------------------------------------------
# AP Chemistry batch B. Same contract as batch A: one exact step over the stated
# decimals, argument order following the sentence, rounding by dec_exact.
#
# `percent_dissociation` is the one relation in the catalogue whose answer is a
# root. It follows rms_speed: the radicand is taken on a fixed integer grid and
# the root by integer square root, so the C solver and this one agree bit for
# bit. It computes the small-x approximation that an exam answer is written
# from, not the quadratic, and says so.
# ---------------------------------------------------------------------------


@relation("percent_dissociation", "chemistry", ["c0", "Ka"], exact=False,
          step=dict(line="percent = 100*sqrt(Ka/c0), small-x approximation",
                    expr="100 * sqrt(@2 / @1)"))
class PercentDissociation:
    @staticmethod
    def gen(rng):
        # Two constraints on the pools, both physical rather than cosmetic.
        #
        # Ka stays at or above 1e-4 because str() of a smaller float is
        # exponent notation: 6.3e-05 in the call line against 0.000063 in the
        # question would not ground against each other.
        #
        # Ka/c0 stays under 3e-4 because the solver is the small-x
        # approximation. The exact quadratic at Ka/c0 = 3e-4 gives 0.87% less
        # than the approximation, so every row the relation emits is inside one
        # percent of the chemistry it names; at the ratio a first draft allowed
        # (0.0025) the gap was 9%, which is not an answer a student should be
        # given as the answer.
        c0 = rng.choice([0.5, 1, 2])
        Ka = rng.choice([ka for ka in (0.0001, 0.00012, 0.00015, 0.00018,
                                       0.00022, 0.00029, 0.0004)
                         if ka / c0 <= 0.0003])
        return [c0, Ka], (
            f"A weak acid of concentration {c0} mol/L has Ka = {Ka}. Find its "
            f"percent dissociation, using the small-x approximation.")

    @staticmethod
    def solve(c0, Ka):
        # The C solver takes the root on a 1e6 grid from the exact ratio; the
        # same integer arithmetic here is what makes the two prints identical.
        r = _exact(Ka) / _exact(c0)
        root = math.isqrt(int(r.p) * 10 ** 12 // int(r.q))
        return f"{dec_exact(sp.Rational(root, 10000))}%"


@relation("mixing_concentration", "chemistry", ["v1", "c1", "v2", "c2"],
          exact=False,
          step=dict(line="c = (c1*v1 + c2*v2) / (v1 + v2)",
                    expr="(@2 * @1 + @4 * @3) / (@1 + @3)"))
class MixingConcentration:
    @staticmethod
    def gen(rng):
        v1 = rng.choice([10, 20, 25, 30, 50, 70, 100])
        c1 = rng.choice([0.1, 0.2, 0.5, 1, 1.5, 2, 3])
        v2 = rng.choice([10, 20, 25, 30, 50, 70, 100])
        c2 = rng.choice([0.1, 0.2, 0.5, 1, 1.5, 2, 3])
        return [v1, c1, v2, c2], (
            f"A volume of {v1} mL of a {c1} mol/L solution is added to {v2} mL "
            f"of a {c2} mol/L solution of the same solute. Find the "
            f"concentration of the mixture.")

    @staticmethod
    def solve(v1, c1, v2, c2):
        # Volume first, because that is how the sentence reads it: "70 mL of
        # 3.0 M is added to 30 mL of 1.0 M". The audit's own shape had the
        # concentrations first, which is the average_rate_of_change trap.
        moles = _exact(c1) * _exact(v1) + _exact(c2) * _exact(v2)
        return f"{dec_exact(moles / (_exact(v1) + _exact(v2)))} mol/L"


@relation("hydrate_water", "chemistry",
          ["mass_hydrate", "mass_anhydrous", "mm_anhydrous", "mm_water"],
          exact=False,
          step=dict(line="x = ((m_h - m_a)/M_w) / (m_a/M_a)",
                    expr="((@1 - @2) / @4) / (@2 / @3)"))
class HydrateWater:
    @staticmethod
    def gen(rng):
        mm_anhydrous = rng.choice([120, 142, 160, 246, 286])
        mm_water = 18
        moles = rng.choice([0.5, 1, 2])
        x = rng.choice([2, 3, 5, 7, 10])
        mass_anhydrous = mm_anhydrous * moles
        mass_hydrate = mass_anhydrous + mm_water * x * moles
        return [mass_hydrate, mass_anhydrous, mm_anhydrous, mm_water], (
            f"A hydrated salt of mass {mass_hydrate} g is heated until the "
            f"water is driven off, leaving {mass_anhydrous} g of the anhydrous "
            f"salt, whose molar mass is {mm_anhydrous} g/mol. Water has molar "
            f"mass {mm_water} g/mol. Find the number of moles of water per "
            f"mole of anhydrous salt.")

    @staticmethod
    def solve(mass_hydrate, mass_anhydrous, mm_anhydrous, mm_water):
        water = (_exact(mass_hydrate) - _exact(mass_anhydrous)) / _exact(mm_water)
        salt = _exact(mass_anhydrous) / _exact(mm_anhydrous)
        return dec_exact(water / salt)


@relation("faraday_deposit", "chemistry",
          ["faradays", "electrons_per_ion", "molar_mass"], exact=False,
          step=dict(line="m = faradays * M / n", expr="@1 * @3 / @2"))
class FaradayDeposit:
    @staticmethod
    def gen(rng):
        faradays = rng.choice([0.1, 0.2, 0.25, 0.5, 1, 1.5, 2])
        electrons = rng.choice([1, 2, 3])
        molar_mass = rng.choice([26.98, 55.85, 58.69, 63.55, 65.38, 107.87,
                                 196.97, 207.2])
        return [faradays, electrons, molar_mass], (
            f"Electrolysis passes {faradays} faraday of charge through a "
            f"solution in which each metal ion takes {electrons} electrons to "
            f"plate out. The metal has molar mass {molar_mass} g/mol. Find the "
            f"mass deposited.")

    @staticmethod
    def solve(faradays, electrons_per_ion, molar_mass):
        return (f"{dec_exact(_exact(faradays) * _exact(molar_mass) / _exact(electrons_per_ion))}"
                f" g")


@relation("rate_factor", "chemistry", ["f1", "o1", "f2", "o2"], exact=False,
          step=dict(line="factor = f1^o1 * f2^o2", expr="@1^@2 * @3^@4"))
class RateFactor:
    @staticmethod
    def gen(rng):
        f1 = rng.choice([2, 3, 4, 0.5])
        o1 = rng.choice([0, 1, 2])
        f2 = rng.choice([2, 3, 4, 0.5])
        o2 = rng.choice([0, 1, 2])
        while o1 == 0 and o2 == 0:
            o2 = rng.choice([0, 1, 2])
        return [f1, o1, f2, o2], (
            f"The first reactant's concentration is multiplied by {f1} and its "
            f"order is {o1}; the second reactant's concentration is multiplied "
            f"by {f2} and its order is {o2}. By what factor does the rate "
            f"change?")

    @staticmethod
    def solve(f1, o1, f2, o2):
        return dec_exact(_exact(f1) ** o1 * _exact(f2) ** o2)


@relation("mass_from_molarity", "chemistry", ["c", "V", "molar_mass"],
          exact=False,
          step=dict(line="m = c * V * M", expr="@1 * @2 * @3"))
class MassFromMolarity:
    @staticmethod
    def gen(rng):
        c = rng.choice([0.1, 0.2, 0.5, 1, 1.5, 2])
        V = rng.choice([0.25, 0.5, 1, 2, 2.5])
        molar_mass = rng.choice([58.5, 98, 100, 142, 180, 58.69])
        return [c, V, molar_mass], (
            f"A volume of {V} L of a {c} mol/L solution contains a solute of "
            f"molar mass {molar_mass} g/mol. Find the mass of solute "
            f"dissolved.")

    @staticmethod
    def solve(c, V, molar_mass):
        return f"{dec_exact(_exact(c) * _exact(V) * _exact(molar_mass))} g"


@relation("ions_from_mixture", "chemistry", ["n1", "ions1", "n2", "ions2"],
          exact=False,
          step=dict(line="n(ion) = n1*i1 + n2*i2", expr="@1 * @2 + @3 * @4"))
class IonsFromMixture:
    """Total moles of one ion from two solutes in the same solution.

    The mirror of `ions_from_moles_formula`, which takes one compound and a
    formula. A mixture states two amounts and two ion counts, and the sum is
    one exact step.
    """

    @staticmethod
    def gen(rng):
        n1 = rng.choice([0.05, 0.1, 0.2, 0.25, 0.5])
        n2 = rng.choice([0.05, 0.1, 0.2, 0.25, 0.5])
        ions1 = rng.choice([1, 2, 3])
        ions2 = rng.choice([1, 2, 3])
        return [n1, ions1, n2, ions2], (
            f"A solution holds {n1} mol of a salt that gives {ions1} of the "
            f"ion in question per formula unit, and {n2} mol of another that "
            f"gives {ions2}. Find the total moles of that ion.")

    @staticmethod
    def solve(n1, ions1, n2, ions2):
        return f"{dec_exact(_exact(n1) * ions1 + _exact(n2) * ions2)} mol"


def _decimal_literal(value):
    """A terminating exact rational as the plain decimal a question states.

    A product of decimal literals is itself a decimal, and str() of the float
    that product became writes binary noise past the ninth place (2.5e-07 came
    out as 2.5000000000000004e-07), which the runtime refuses and no question
    ever prints.
    """
    value = sp.Rational(value)
    rest, twos, fives = value.q, 0, 0
    while rest % 2 == 0:
        rest //= 2
        twos += 1
    while rest % 5 == 0:
        rest //= 5
        fives += 1
    digits = max(twos, fives)
    if rest != 1 or digits > 9:
        raise ValueError("%s has no decimal form the runtime parses" % value)
    scaled = value.p * (10 ** digits // value.q)
    text = str(scaled).rjust(digits + 1, "0")
    if digits == 0:
        return text
    return "%s.%s" % (text[:-digits], text[-digits:])


@relation("excess_moles", "chemistry", ["v1", "c1", "v2", "c2"], exact=False)
class ExcessMoles:
    """The amount of reagent left over when two solutions are mixed.

    One-to-one pairing, which is what the bank's neutralisation and
    precipitation rows ask: n(left) = |c1*V1 - c2*V2|, with the volumes in mL
    so the magnitudes are the numbers the question writes. The answer is the
    excess whichever side it falls on, because every row wants how much is
    left, not which reagent it was. No step template: a difference of two
    products is not the substitution the working pane renders.
    """

    @staticmethod
    def gen(rng):
        while True:
            v1 = rng.choice([10, 20, 25, 50, 100])
            c1 = rng.choice([0.1, 0.2, 0.25, 0.5])
            v2 = rng.choice([10, 20, 25, 50, 100])
            c2 = rng.choice([0.1, 0.2, 0.25, 0.5])
            if v1 * c1 != v2 * c2:
                break
        return [v1, c1, v2, c2], (
            f"A {v1} mL sample of {c1} M solution is mixed with {v2} mL of "
            f"{c2} M solution and the two react one to one. How many moles of "
            f"the excess reagent are left over?")

    @staticmethod
    def solve(v1, c1, v2, c2):
        left = abs(_exact(v1) * _exact(c1) - _exact(v2) * _exact(c2))
        return f"{dec_exact(left / 1000)} mol"


@relation("excess_concentration", "chemistry", ["v1", "c1", "v2", "c2"],
          exact=False)
class ExcessConcentration:
    """The concentration of the reagent left over after two solutions mix.

    The mirror of `mixing_concentration`, which takes these same four arguments
    and blends both solutes; this one keeps only the excess of a one-to-one
    reaction and spreads it over the combined volume:

        c(left) = |c1*V1 - c2*V2| / (V1 + V2)

    Millilitres over millilitres is mol/L with no factor, which is why the
    volumes are declared in mL. Every argument and both volumes are numbers the
    question writes, which the plain subtraction's total volume is not. No step
    template: a difference of products over a sum is not the substitution the
    working pane renders.
    """

    @staticmethod
    def gen(rng):
        while True:
            v1 = rng.choice([10, 20, 25, 50, 100])
            c1 = rng.choice([0.1, 0.2, 0.25, 0.5])
            v2 = rng.choice([10, 20, 25, 50, 100])
            c2 = rng.choice([0.1, 0.2, 0.25, 0.5])
            if v1 * c1 != v2 * c2:
                break
        return [v1, c1, v2, c2], (
            f"A {v1} mL volume of {c1} M solution is mixed with {v2} mL of "
            f"{c2} M solution and the two react one to one. Find the "
            f"concentration of the reagent left over in the mixture.")

    @staticmethod
    def solve(v1, c1, v2, c2):
        left = abs(_exact(v1) * _exact(c1) - _exact(v2) * _exact(c2))
        return f"{dec_exact(left / (_exact(v1) + _exact(v2)))} mol/L"


@relation("ion_from_ksp", "chemistry", ["ksp", "other", "count"], exact=False)
class IonFromKsp:
    """One ion's concentration from Ksp and the other ion's concentration.

    Ksp = [other]^m * [ion]^n, so [ion] = (Ksp/[other])^(1/n). Only a square
    root is computed, because that is the only power the bank's rows use and an
    exact grid root is worth more here than a general case nobody asked for.
    No step template: a root is not arithmetic over the arguments, and the
    runtime prints the same four figures the reference does.

    `gen` draws a Ksp of 1e-4 or more, because that is the smallest value the
    wording layer writes without an exponent (`numbers.SMALLEST`). The solvers
    themselves are exact for any positive Ksp the runtime parses: the bank's own
    row states 4.0 x 10-8 and computes to 0.2 mol/L. What is missing is a
    question generator that can write 4.0 x 10-8, not the arithmetic.
    """

    @staticmethod
    def gen(rng):
        count = 2
        other = rng.choice([0.01, 0.02, 0.05, 0.1])
        ion = rng.choice([0.2, 0.25, 0.5])
        ksp = _decimal_literal(_exact(other) * _exact(ion) * _exact(ion))
        return [ksp, other, count], (
            f"A saturated solution holds {other} mol/L of one ion, and the "
            f"compound gives {count} of the other ion per formula unit. Ksp is "
            f"{ksp}. Find the concentration of the other ion.")

    @staticmethod
    def solve(ksp, other, count):
        r = _exact(ksp) / _exact(other)
        if r <= 0 or int(count) != 2:
            raise ValueError("only a square root, and only for positive inputs")
        scaled = r.p * 1000000000000 // r.q
        if scaled > 8000000000000000000:
            raise ValueError("past the exact grid")
        return f"{dec_exact(sp.Rational(math.isqrt(scaled), 1000000))} mol/L"


@relation("kc_from_ice", "chemistry", ["initial", "product", "coefficient"],
          exact=False,
          step=dict(line="Kc = [prod] / ([init] - c*[prod])^c",
                    expr="@2 / ((@1 - @3 * @2) ^ @3)"))
class KcFromIce:
    """Kc from an ICE row's two givens, in one call.

    The bank's equilibrium rows give an initial amount and the amount of
    product at equilibrium, and the arithmetic in between — subtract the
    change to get the reactant's equilibrium concentration — is what a chain of
    relations cannot express and a single relation can.
    """

    @staticmethod
    def gen(rng):
        coefficient = rng.choice([1, 2])
        product = rng.choice([0.002, 0.005, 0.01, 0.02, 0.0133, 0.25])
        initial = product * coefficient * rng.choice([2, 3, 4, 5])
        return [initial, product, coefficient], (
            f"A rigid vessel is charged so that the initial concentration of "
            f"the reactant is {initial} mol/L. At equilibrium the product is "
            f"{product} mol/L. The reaction makes {coefficient} mol of product "
            f"per mol of reactant. Find Kc.")

    @staticmethod
    def solve(initial, product, coefficient):
        c = int(coefficient)
        if c < 1 or c > 4:
            raise ValueError("coefficient out of range")
        left = _exact(initial) - c * _exact(product)
        if left <= 0:
            raise ValueError("no equilibrium: the change exceeds the initial")
        return dec_exact(_exact(product) / (left ** c))


@relation("moles_from_molarity", "chemistry", ["c", "V"],
          exact=False,
          step=dict(line="n = c * V", expr="@1 * @2"))
class MolesFromMolarity:
    """The inverse of `molarity`, and the first step of most titration rows.

    Measured on the bank: nine of the 23 second-computation rows need the
    number of moles in a volume of a solution before anything else can happen,
    and no relation produced it. `molarity` solves the other way (c from n and
    V), so a chain could not borrow it: the graph admits the edge and the
    question needs the reverse.
    """

    @staticmethod
    def gen(rng):
        c = rng.choice([0.05, 0.1, 0.2, 0.25, 0.5, 1, 1.5, 2])
        V = rng.choice([0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2])
        return [c, V], (
            f"A volume of {V} L of a solution of concentration {c} mol/L is "
            f"measured out. Find the number of moles of solute in it.")

    @staticmethod
    def solve(c, V):
        return f"{dec_exact(_exact(c) * _exact(V))} mol"


# ---------------------------------------------------------------------------
# AP Chemistry batch C. Five of these are one exact step each, like the two
# batches before them. The sixth, vsepr_bond_angle, is a table rather than a
# computation and carries no `step`: there is no working to show for a lookup,
# and a fabricated `expr` is exactly the plausible-wrong display check_steps.py
# exists to catch.
# ---------------------------------------------------------------------------


@relation("gas_mixture_pressure", "chemistry", ["P_i", "V_i", "V_f"],
          exact=False,
          step=dict(line="P_f = P_i * V_i / V_f", expr="@1 * @2 / @3"))
class GasMixturePressure:
    @staticmethod
    def gen(rng):
        P_i = rng.choice([0.5, 1, 1.5, 2, 3, 4])
        V_i = rng.choice([1, 2, 5, 10, 20])
        V_f = rng.choice([1, 2, 5, 10, 20, 25, 50])
        while V_f == V_i:
            V_f = rng.choice([1, 2, 5, 10, 20, 25, 50])
        return [P_i, V_i, V_f], (
            f"One component of a gas mixture exerts {P_i} atm while it occupies "
            f"{V_i} L. The mixture is transferred to a {V_f} L vessel at "
            f"constant temperature. Find that component's new partial "
            f"pressure.")

    @staticmethod
    def solve(P_i, V_i, V_f):
        return f"{dec_exact(_exact(P_i) * _exact(V_i) / _exact(V_f))} atm"


@relation("mixture_final_temperature", "chemistry", ["m1", "T1", "m2", "T2"],
          exact=False,
          step=dict(line="T = (m1*T1 + m2*T2) / (m1 + m2)",
                    expr="(@1 * @2 + @3 * @4) / (@1 + @3)"))
class MixtureFinalTemperature:
    @staticmethod
    def gen(rng):
        m1 = rng.choice([10, 20, 25, 50, 75, 100, 150, 200])
        T1 = rng.choice([280, 285, 290, 295, 300, 310, 320, 340, 350, 373])
        m2 = rng.choice([10, 20, 25, 50, 75, 100, 150, 200])
        T2 = rng.choice([280, 285, 290, 295, 300, 310, 320, 340, 350, 373])
        while T2 == T1:
            T2 = rng.choice([280, 285, 290, 295, 300, 310, 320, 340, 350, 373])
        return [m1, T1, m2, T2], (
            f"A mass of {m1} g of water at {T1} K is mixed with {m2} g of water "
            f"at {T2} K in an insulated container. Find the final temperature.")

    @staticmethod
    def solve(m1, T1, m2, T2):
        total = _exact(m1) * _exact(T1) + _exact(m2) * _exact(T2)
        return f"{dec_exact(total / (_exact(m1) + _exact(m2)))} K"


@relation("stoich_volume_gas", "chemistry", ["V_given", "ratio"], exact=False,
          step=dict(line="V = V_given * ratio", expr="@1 * @2"))
class StoichVolumeGas:
    @staticmethod
    def gen(rng):
        V_given = rng.choice([1, 2, 2.5, 5, 10, 20, 22.4, 44.8])
        ratio = rng.choice([1, 2, 3, 4, 5])
        return [V_given, ratio], (
            f"{V_given} L of a gaseous reactant is consumed at constant "
            f"temperature and pressure. Each mole of it produces {ratio} mol of "
            f"gaseous product. Find the volume of product formed.")

    @staticmethod
    def solve(V_given, ratio):
        return f"{dec_exact(_exact(V_given) * _exact(ratio))} L"


@relation("mass_from_percent_composition", "chemistry",
          ["percent", "total_mass"], exact=False,
          step=dict(line="m = percent / 100 * total_mass", expr="@1 / 100 * @2"))
class MassFromPercentComposition:
    @staticmethod
    def gen(rng):
        percent = rng.choice([5, 10, 12.5, 20, 25, 40, 50, 62.5])
        total_mass = rng.choice([10, 20, 50, 100, 200, 250, 500])
        return [percent, total_mass], (
            f"A sample is {percent} percent of one element by mass and has a "
            f"total mass of {total_mass} g. Find the mass of that element in "
            f"the sample.")

    @staticmethod
    def solve(percent, total_mass):
        return f"{dec_exact(_exact(percent) / 100 * _exact(total_mass))} g"


@relation("ph_of_water", "chemistry", ["pkw"], exact=False,
          step=dict(line="pH = pKw / 2", expr="@1 / 2"))
class PhOfWater:
    @staticmethod
    def gen(rng):
        # pKw, not Kw. The audit asked for (Kw, T); Kw at 25 C is 1e-14 and the
        # runtime's logarithm takes a denominator up to 1e9, so Kw would have to
        # be rescaled before it could be read at all. pKw is the same
        # information, is what a table prints, and makes the answer one exact
        # halving: [H+] = sqrt(Kw), so pH = pKw/2 with no root and no log.
        pkw = rng.choice([12.26, 13.26, 13.62, 13.99, 14, 14.17, 14.94])
        return [pkw], (
            f"At a particular temperature the ionic product of water makes "
            f"pKw = {pkw}. Find the pH of pure water at that temperature.")

    @staticmethod
    def solve(pkw):
        return dec_exact(_exact(pkw) / 2)


VSEPR_ANGLES = {
    (2, 0): sp.Rational(180),
    (3, 0): sp.Rational(120),
    (4, 0): sp.Rational(1095, 10),
    (3, 1): sp.Rational(107),
    (2, 2): sp.Rational(1045, 10),
    (6, 0): sp.Rational(90),
}


@relation("vsepr_bond_angle", "chemistry",
          ["electron_domains", "lone_pairs"], exact=False)
class VseprBondAngle:
    """The bond angle VSEPR assigns to a shape, read from a table.

    This computes nothing: it looks the shape up. Shapes with two distinct bond
    angles, such as the trigonal bipyramid, are deliberately absent, because a
    single-number answer for them would have to choose between 90 and 120
    degrees and would be wrong about half the bonds either way. Anything not in
    the table is refused rather than guessed.
    """

    @staticmethod
    def gen(rng):
        domains, lone = rng.choice(sorted(VSEPR_ANGLES))
        return [domains, lone], (
            f"A molecule has {domains} electron domains around its central atom "
            f"and {lone} of them are lone pairs. Find its bond angle in "
            f"degrees, from the VSEPR table.")

    @staticmethod
    def solve(electron_domains, lone_pairs):
        key = (int(electron_domains), int(lone_pairs))
        if key not in VSEPR_ANGLES:
            raise ValueError(f"no single bond angle is tabulated for {key}")
        return dec_exact(VSEPR_ANGLES[key])
