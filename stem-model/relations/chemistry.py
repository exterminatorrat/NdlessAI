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


def _num(value):
    """A derived quantity as the int or decimal token the question shows."""

    r = _exact(value)
    return int(r) if r.q == 1 else float(r)

@relation("dilution_v1", "chemistry", ["m1", "m2", "v2"], exact=False)
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

@relation("h_from_ph", "chemistry", ["ph"], exact=False)
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

@relation("buffer_ph", "chemistry", ["pka", "base", "acid"], exact=False)
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

@relation("calorimetry_mass", "chemistry", ["q", "c", "dT"], exact=False)
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

@relation("calorimetry_c", "chemistry", ["q", "m", "dT"], exact=False)
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

@relation("first_law", "chemistry", ["q", "w"], exact=False)
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

@relation("gibbs", "chemistry", ["dH", "T", "dS"], exact=False)
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
          step=dict(line="eff = (Th - Tc)/Th * 100", expr="(@1 - @2) / @1 * 100"))
class Carnot:
    @staticmethod
    def gen(rng):
        t_cold = rng.choice([200, 250, 300, 350, 400, 500])
        t_hot = t_cold + rng.choice([100, 150, 200, 300, 400, 500])
        return [t_hot, t_cold], (f"A Carnot engine operates between {t_hot} K "
                                 f"and {t_cold} K. Find its efficiency.")

    @staticmethod
    def solve(t_hot, t_cold):
        return f"{dec_exact(sp.Rational(100 * (t_hot - t_cold), t_hot))}%"

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
          ["mass", "mm_given", "mm_product", "ratio"], exact=False)
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
