import random

import sympy as sp


class Relation:
    def __init__(self, name, subject, params, gen, solve, exact):
        self.name = name
        self.subject = subject
        self.params = tuple(params)
        self.gen = gen
        self.solve = solve
        self.exact = exact


RELATIONS = {}


def relation(name, subject, params, exact=True):
    def register(cls):
        RELATIONS[name] = Relation(name, subject, params, cls.gen, cls.solve, exact)
        return cls

    return register


def fmt(value):
    if isinstance(value, (int, sp.Integer)):
        return str(int(value))
    if isinstance(value, sp.Rational):
        return str(value)
    return dec(value)


def _exact(value):
    if isinstance(value, (int, sp.Integer)):
        return sp.Rational(int(value))
    if isinstance(value, sp.Rational):
        return sp.Rational(value)
    if isinstance(value, sp.Float):
        return sp.Rational(str(value))
    if isinstance(value, float):
        return sp.Rational(repr(value))
    return sp.Rational(str(sp.N(value, 20)))


def dec(value):
    return f"{float(value):.4g}"


def dec_exact(value):
    """Four significant digits, round half to even, matching dec4_render.

    Use this when the answer is an exact rational of the decimal inputs, as
    with a physical constant like 9.81. There the C renderer rounds the exact
    rational while %.4g rounds the nearest double, and the two disagree on
    ties: 15*3*9.81 is exactly 441.45, so the device shows 441.4 and %.4g
    gives 441.5. Rounding the exact rational here keeps them identical.

    Do not use this for irrational results such as a sine or a square root.
    There the double is the better approximation of the true value and
    matches the device's integer approximation more often.
    """

    r = _exact(value)
    if r == 0:
        return "0"
    neg = r < 0
    r = abs(r)
    e = 0
    while r >= 10:
        r /= 10
        e += 1
    while r < 1:
        r *= 10
        e -= 1
    x = r * 10000
    n = int(x)
    rem = x - n
    d = [int(c) for c in str(n).rjust(5, "0")]
    up = 0
    if d[4] > 5 or (d[4] == 5 and rem > 0):
        up = 1
    elif d[4] == 5:
        up = d[3] & 1
    if up:
        v = int("".join(str(d[i]) for i in range(4))) + 1
        if v >= 10000:
            v //= 10
            e += 1
        d = [int(c) for c in str(v).rjust(4, "0")] + [0]
    else:
        d[4] = 0
    if e >= 4 or e <= -5:
        fz = 3
        while fz > 0 and d[fz] == 0:
            fz -= 1
        s = str(d[0])
        if fz > 0:
            s += "." + "".join(str(d[i]) for i in range(1, fz + 1))
        s += "e" + ("-" if e < 0 else "+") + str(abs(e)).rjust(2, "0")
    elif e >= 0:
        s = "".join(str(d[i]) for i in range(e + 1))
        fz = 3
        while fz > e and d[fz] == 0:
            fz -= 1
        if fz > e:
            s += "." + "".join(str(d[i]) for i in range(e + 1, fz + 1))
    else:
        fz = 3
        while fz > 0 and d[fz] == 0:
            fz -= 1
        s = "0." + "0" * (-e - 1) + "".join(str(d[i]) for i in range(fz + 1))
    return ("-" if neg else "") + s


def poly(coeffs, var="x"):
    terms = []
    deg = len(coeffs) - 1
    for i, c in enumerate(coeffs):
        d = deg - i
        if c == 0:
            continue
        mag = abs(c)
        if d == 0:
            body = str(mag)
        elif d == 1:
            body = ("" if mag == 1 else str(mag)) + var
        else:
            body = ("" if mag == 1 else str(mag)) + f"{var}^{d}"
        if not terms:
            terms.append(("-" if c < 0 else "") + body)
        else:
            terms.append((" - " if c < 0 else " + ") + body)
    return "".join(terms) or "0"


@relation("quad_solve", "precalc", ["a", "b", "c"])
class QuadSolve:
    @staticmethod
    def gen(rng):
        r1, r2 = rng.randint(-6, 6), rng.randint(-6, 6)
        a = rng.choice([1, 1, 1, 2, -1, 3])
        b, c = -a * (r1 + r2), a * r1 * r2
        q = f"Solve {poly([a, b, c])} = 0 for x."
        return [a, b, c], q

    @staticmethod
    def solve(a, b, c):
        roots = sp.solve(sp.Eq(a * sp.Symbol("x") ** 2 + b * sp.Symbol("x") + c, 0))
        return ", ".join(f"x={fmt(r)}" for r in sorted(roots, key=sp.re))


@relation("deriv_poly3", "calculus", ["a", "b", "c", "d", "x"])
class DerivPoly3:
    @staticmethod
    def gen(rng):
        a, b, c, d = (rng.randint(-4, 4) for _ in range(4))
        x = rng.randint(-3, 3)
        q = f"Evaluate the derivative of f(x) = {poly([a, b, c, d])} at x = {x}."
        return [a, b, c, d, x], q

    @staticmethod
    def solve(a, b, c, d, x):
        var = sp.Symbol("x")
        f = a * var ** 3 + b * var ** 2 + c * var + d
        return fmt(sp.diff(f, var).subs(var, x))


@relation("integr_poly3", "calculus", ["a", "b", "c", "d", "lo", "hi"])
class IntegPoly3:
    @staticmethod
    def gen(rng):
        a, b, c, d = (rng.randint(-3, 3) for _ in range(4))
        lo = rng.randint(-2, 2)
        hi = lo + rng.randint(1, 4)
        q = (f"Compute the definite integral of {poly([a, b, c, d])} "
             f"from x = {lo} to x = {hi}.")
        return [a, b, c, d, lo, hi], q

    @staticmethod
    def solve(a, b, c, d, lo, hi):
        var = sp.Symbol("x")
        f = a * var ** 3 + b * var ** 2 + c * var + d
        return fmt(sp.integrate(f, (var, lo, hi)))


@relation("linear2", "precalc", ["a1", "b1", "c1", "a2", "b2", "c2"])
class Linear2:
    @staticmethod
    def gen(rng):
        while True:
            x, y = rng.randint(-5, 5), rng.randint(-5, 5)
            a1, b1, a2, b2 = (rng.randint(-4, 4) or 1 for _ in range(4))
            if a1 * b2 - a2 * b1:
                break
        c1, c2 = a1 * x + b1 * y, a2 * x + b2 * y
        q = (f"Solve the system {a1}x + {b1}y = {c1} and {a2}x + {b2}y = {c2}.")
        return [a1, b1, c1, a2, b2, c2], q

    @staticmethod
    def solve(a1, b1, c1, a2, b2, c2):
        x, y = sp.symbols("x y")
        sol = sp.solve([sp.Eq(a1 * x + b1 * y, c1), sp.Eq(a2 * x + b2 * y, c2)], [x, y])
        return f"x={fmt(sol[x])}, y={fmt(sol[y])}"


@relation("exp_solve", "precalc", ["base", "k"])
class ExpSolve:
    @staticmethod
    def gen(rng):
        base = rng.choice([2, 3, 5, 10])
        k = base ** rng.randint(2, 4)
        return [base, k], f"Solve {base}^x = {k} for x."

    @staticmethod
    def solve(base, k):
        return fmt(sp.log(k) / sp.log(base))


@relation("kinematic_v", "physics", ["v0", "a", "t"], exact=False)
class KinematicV:
    @staticmethod
    def gen(rng):
        v0, a, t = rng.randint(0, 20), rng.randint(-5, 8), rng.randint(1, 10)
        q = (f"A body starts at {v0} m/s with constant acceleration {a} m/s^2. "
             f"What is its velocity after {t} s?")
        return [v0, a, t], q

    @staticmethod
    def solve(v0, a, t):
        return f"{dec(sp.Float(v0 + a * t))} m/s"


@relation("proj_range", "physics", ["v0", "theta", "g"], exact=False)
class ProjRange:
    @staticmethod
    def gen(rng):
        v0 = rng.randrange(5, 40)
        theta = rng.choice([15, 20, 25, 30, 35, 40, 45, 50, 55, 60])
        g = rng.choice([9.8, 9.81, 10])
        q = (f"A projectile is launched at {v0} m/s at {theta} degrees above the "
             f"horizontal. What horizontal range does it cover with g = {g} m/s^2?")
        return [v0, theta, g], q

    @staticmethod
    def solve(v0, theta, g):
        rad = sp.rad(sp.Integer(theta))
        r = sp.Integer(v0) ** 2 * sp.sin(2 * rad) / sp.Float(g)
        return f"{dec(r)} m"


@relation("ohm_i", "physics", ["V", "R"], exact=False)
class OhmI:
    @staticmethod
    def gen(rng):
        V, R = rng.randint(1, 24), rng.choice([2, 3, 4, 5, 6, 8, 10, 12])
        return [V, R], f"A {R} ohm resistor has {V} V across it. What current flows?"

    @staticmethod
    def solve(V, R):
        return f"{dec(sp.Float(V) / sp.Float(R))} A"


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


@relation("sin_eval", "trig", ["a"], exact=False)
class SinEval:
    @staticmethod
    def gen(rng):
        a = rng.choice([0, 5, 10, 12, 15, 20, 25, 30, 35, 37, 40, 45,
                        50, 53, 55, 60, 65, 70, 75, 80, 85, 90])
        return [a], f"What is sin({a})?"

    @staticmethod
    def solve(a):
        return dec(sp.sin(sp.rad(a)))


@relation("cos_eval", "trig", ["a"], exact=False)
class CosEval:
    @staticmethod
    def gen(rng):
        a = rng.choice([0, 5, 10, 12, 15, 20, 25, 30, 35, 37, 40, 45,
                        50, 53, 55, 60, 65, 70, 75, 80, 85, 90])
        return [a], f"What is cos({a})?"

    @staticmethod
    def solve(a):
        return dec(sp.cos(sp.rad(a)))


@relation("tan_eval", "trig", ["a"], exact=False)
class TanEval:
    @staticmethod
    def gen(rng):
        a = rng.choice([0, 5, 10, 12, 15, 20, 25, 30, 35, 37, 40, 45,
                        50, 53, 55, 60, 65, 70, 75, 80, 85])
        return [a], f"What is tan({a})?"

    @staticmethod
    def solve(a):
        return dec(sp.tan(sp.rad(a)))


@relation("pythagoras", "trig", ["a", "b"], exact=False)
class Pythagoras:
    @staticmethod
    def gen(rng):
        a, b = rng.randint(3, 20), rng.randint(3, 20)
        return [a, b], (f"A right triangle has legs {a} and {b}. "
                        f"Find the hypotenuse.")

    @staticmethod
    def solve(a, b):
        return dec(sp.sqrt(a ** 2 + b ** 2))


@relation("law_sines", "trig", ["a", "A", "B"], exact=False)
class LawSines:
    @staticmethod
    def gen(rng):
        a = rng.randint(4, 30)
        A = rng.choice([30, 40, 50, 60, 70, 80])
        B = rng.choice([20, 30, 40, 50, 60, 70])
        return [a, A, B], (f"In a triangle, side a = {a} is opposite angle "
                           f"A = {A} degrees, and angle B = {B} degrees. "
                           f"Find side b.")

    @staticmethod
    def solve(a, A, B):
        b = sp.Float(a) * sp.sin(sp.rad(B)) / sp.sin(sp.rad(A))
        return dec(b)


@relation("law_cosines", "trig", ["a", "b", "C"], exact=False)
class LawCosines:
    @staticmethod
    def gen(rng):
        a, b = rng.randint(3, 20), rng.randint(3, 20)
        C = rng.choice([30, 45, 60, 90, 120, 135])
        return [a, b, C], (f"A triangle has sides {a} and {b} with an "
                           f"included angle of {C} degrees. "
                           f"Find the third side.")

    @staticmethod
    def solve(a, b, C):
        c2 = a ** 2 + b ** 2 - 2 * a * b * sp.cos(sp.rad(C))
        return dec(sp.sqrt(c2))


@relation("arc_length", "trig", ["r", "theta"], exact=False)
class ArcLength:
    @staticmethod
    def gen(rng):
        r = rng.randint(1, 20)
        theta = rng.choice([30, 45, 60, 90, 120, 135, 180, 270, 360])
        return [r, theta], (f"A circle of radius {r} has an arc subtending "
                            f"{theta} degrees. Find the arc length.")

    @staticmethod
    def solve(r, theta):
        return dec(sp.Float(r) * sp.Float(theta) * sp.pi / 180)


@relation("log_eval", "logs", ["base", "x"], exact=False)
class LogEval:
    @staticmethod
    def gen(rng):
        base = rng.choice([2, 3, 4, 5, 10])
        x = rng.choice([2, 3, 4, 5, 6, 8, 9, 10, 12, 16, 20, 25, 27,
                        32, 36, 49, 64, 81, 100, 125, 128, 243, 1000])
        return [base, x], f"What is log base {base} of {x}?"

    @staticmethod
    def solve(base, x):
        return dec(sp.log(x, base))


@relation("ln_eval", "logs", ["x"], exact=False)
class LnEval:
    @staticmethod
    def gen(rng):
        x = rng.choice([2, 3, 4, 5, 6, 8, 10, 12, 15, 20, 25, 30, 50, 100])
        return [x], f"What is ln({x})?"

    @staticmethod
    def solve(x):
        return dec(sp.log(x))


@relation("log10_eval", "logs", ["x"], exact=False)
class Log10Eval:
    @staticmethod
    def gen(rng):
        x = rng.choice([2, 3, 4, 5, 6, 8, 10, 12, 15, 20, 25, 30, 50,
                        100, 1000])
        return [x], f"What is log({x})?"

    @staticmethod
    def solve(x):
        return dec(sp.log(x, 10))


@relation("compound_interest", "logs", ["P", "r", "n", "t"], exact=False)
class CompoundInterest:
    @staticmethod
    def gen(rng):
        P = rng.choice([100, 200, 500, 1000, 1500, 2000, 5000])
        r = rng.choice([2, 3, 4, 5, 6, 8])
        n = rng.choice([1, 2, 4, 12])
        t = rng.randint(1, 10)
        return [P, r, n, t], (f"{P} dollars is invested at {r}% annual "
                              f"interest compounded {n} times per year for "
                              f"{t} years. Find the balance.")

    @staticmethod
    def solve(P, r, n, t):
        A = sp.Rational(P) * (1 + sp.Rational(r, 100 * n)) ** (n * t)
        return dec_exact(A)


@relation("continuous_growth", "logs", ["P", "r", "t"], exact=False)
class ContinuousGrowth:
    @staticmethod
    def gen(rng):
        P = rng.choice([100, 200, 500, 1000, 2000])
        r = rng.choice([2, 3, 4, 5, 6, 8, 10])
        t = rng.randint(1, 10)
        return [P, r, t], (f"A population of {P} grows continuously at "
                           f"{r}% per year for {t} years. "
                           f"Find the population.")

    @staticmethod
    def solve(P, r, t):
        return dec(sp.Float(P) * sp.exp(sp.Float(r) / 100 * t))


@relation("half_life", "logs", ["initial", "hl", "t"], exact=False)
class HalfLife:
    @staticmethod
    def gen(rng):
        initial = rng.choice([100, 200, 400, 500, 800, 1000])
        hl = rng.choice([2, 3, 4, 5, 6, 10, 12])
        t = rng.randint(1, 30)
        return [initial, hl, t], (f"A sample of {initial} g has a half life "
                                  f"of {hl} years. How much remains after "
                                  f"{t} years?")

    @staticmethod
    def solve(initial, hl, t):
        return dec(sp.Float(initial) * sp.Float("0.5") ** (sp.Float(t) / hl))


@relation("doubling_time", "logs", ["r"], exact=False)
class DoublingTime:
    @staticmethod
    def gen(rng):
        r = rng.choice([2, 3, 4, 5, 6, 7, 8, 10, 12])
        return [r], (f"An investment grows at {r}% per year. "
                     f"How many years does it take to double?")

    @staticmethod
    def solve(r):
        return dec(sp.log(2) / sp.log(1 + sp.Float(r) / 100))


@relation("arith_seq", "algebra", ["a1", "d", "n"], exact=True)
class ArithSeq:
    @staticmethod
    def gen(rng):
        a1 = rng.randint(1, 20)
        d = rng.choice([-5, -4, -3, -2, -1, 1, 2, 3, 4, 5])
        n = rng.randint(2, 20)
        return [a1, d, n], (f"An arithmetic sequence starts at {a1} with "
                            f"common difference {d}. Find the {n}th term "
                            f"and the sum of the first {n} terms.")

    @staticmethod
    def solve(a1, d, n):
        an = a1 + (n - 1) * d
        sn = n * (a1 + an) // 2
        return f"a_n={an}, S_n={sn}"


@relation("geom_seq", "algebra", ["a1", "r", "n"], exact=True)
class GeomSeq:
    @staticmethod
    def gen(rng):
        a1 = rng.randint(1, 10)
        r = rng.choice([2, 3, 4])
        n = rng.randint(2, 8)
        return [a1, r, n], (f"A geometric sequence starts at {a1} with "
                            f"common ratio {r}. Find the {n}th term and the "
                            f"sum of the first {n} terms.")

    @staticmethod
    def solve(a1, r, n):
        an = a1 * r ** (n - 1)
        sn = a1 * (r ** n - 1) // (r - 1)
        return f"a_n={an}, S_n={sn}"


@relation("abs_value_eq", "algebra", ["a", "b", "c"], exact=True)
class AbsValueEq:
    @staticmethod
    def gen(rng):
        a = rng.randint(1, 6)
        b = rng.randint(-10, 10)
        c = rng.randint(1, 20)
        return [a, b, c], f"Solve |{a}x+{b}| = {c} for x."

    @staticmethod
    def solve(a, b, c):
        r1 = sp.Rational(c - b, a)
        r2 = sp.Rational(-c - b, a)
        lo, hi = sorted([r1, r2])
        return f"x={fmt(lo)}, x={fmt(hi)}"


@relation("linear_ineq", "algebra", ["a", "b", "c"], exact=True)
class LinearIneq:
    @staticmethod
    def gen(rng):
        a = rng.choice([-5, -4, -3, -2, -1, 1, 2, 3, 4, 5])
        b = rng.randint(-10, 10)
        c = rng.randint(-10, 20)
        return [a, b, c], f"Solve {a}x+{b} < {c} for x."

    @staticmethod
    def solve(a, b, c):
        bound = sp.Rational(c - b, a)
        op = "<" if a > 0 else ">"
        return f"x {op} {fmt(bound)}"


@relation("system2", "algebra", ["a1", "b1", "c1", "a2", "b2", "c2"],
          exact=True)
class System2:
    @staticmethod
    def gen(rng):
        while True:
            a1, b1 = rng.randint(-5, 5), rng.randint(-5, 5)
            a2, b2 = rng.randint(-5, 5), rng.randint(-5, 5)
            if a1 * b2 - a2 * b1 != 0:
                break
        c1, c2 = rng.randint(-10, 10), rng.randint(-10, 10)
        return [a1, b1, c1, a2, b2, c2], (
            f"Solve the system {a1}x+{b1}y={c1} and {a2}x+{b2}y={c2}.")

    @staticmethod
    def solve(a1, b1, c1, a2, b2, c2):
        det = a1 * b2 - a2 * b1
        x = sp.Rational(c1 * b2 - c2 * b1, det)
        y = sp.Rational(a1 * c2 - a2 * c1, det)
        return f"x={fmt(x)}, y={fmt(y)}"


@relation("complete_square", "algebra", ["a", "b", "c"], exact=True)
class CompleteSquare:
    @staticmethod
    def gen(rng):
        a = rng.choice([1, 1, 1, 2, 3])
        b = rng.randint(-10, 10)
        c = rng.randint(-10, 10)
        return [a, b, c], (f"Complete the square for {a}x^2+{b}x+{c} and "
                           f"state the vertex.")

    @staticmethod
    def solve(a, b, c):
        h = sp.Rational(-b, 2 * a)
        k = sp.Rational(c) - sp.Rational(b * b, 4 * a)
        return f"vertex=({fmt(h)}, {fmt(k)})"


@relation("factor_quadratic", "algebra", ["a", "b", "c"], exact=True)
class FactorQuadratic:
    @staticmethod
    def gen(rng):
        a = rng.choice([1, 1, 1, 2, 3])
        r1 = rng.randint(-6, 6)
        r2 = rng.randint(-6, 6)
        b = -a * (r1 + r2)
        c = a * r1 * r2
        return [a, b, c], f"Factor {a}x^2+{b}x+{c}."

    @staticmethod
    def solve(a, b, c):
        r1 = sp.Rational(-b + sp.sqrt(b * b - 4 * a * c), 2 * a)
        r2 = sp.Rational(-b - sp.sqrt(b * b - 4 * a * c), 2 * a)
        lo, hi = sorted([r1, r2])
        lead = "" if a == 1 else str(a)
        return f"{lead}{_factor(lo)}{_factor(hi)}"


def _factor(r):
    if r == 0:
        return "x"
    if r > 0:
        return f"(x-{fmt(r)})"
    return f"(x+{fmt(-r)})"


@relation("kinetic_energy", "physics", ["m", "v"], exact=False)
class KineticEnergy:
    @staticmethod
    def gen(rng):
        m = rng.choice([1, 2, 3, 4, 5, 8, 10, 12, 15, 20])
        v = rng.choice([2, 3, 4, 5, 6, 8, 10, 12, 15, 20])
        return [m, v], (f"A {m} kg object moves at {v} m/s. "
                        f"Find its kinetic energy.")

    @staticmethod
    def solve(m, v):
        return f"{dec(sp.Rational(m * v * v, 2))} J"


@relation("potential_energy", "physics", ["m", "h", "g"], exact=False)
class PotentialEnergy:
    @staticmethod
    def gen(rng):
        m = rng.choice([1, 2, 3, 4, 5, 8, 10, 12, 15, 20])
        h = rng.choice([1, 2, 3, 4, 5, 6, 8, 10, 12, 15, 20])
        g = rng.choice([9.8, 9.81, 10])
        return [m, h, g], (f"A {m} kg mass is raised {h} m. Find its "
                           f"gravitational potential energy using g = {g}.")

    @staticmethod
    def solve(m, h, g):
        return f"{dec_exact(sp.Rational(m) * sp.Rational(h) * _exact(g))} J"


@relation("work", "physics", ["F", "d", "theta"], exact=False)
class Work:
    @staticmethod
    def gen(rng):
        F = rng.choice([10, 20, 25, 30, 40, 50, 60, 80, 100])
        d = rng.choice([2, 3, 4, 5, 6, 8, 10, 12, 15, 20])
        theta = rng.choice([0, 30, 45, 60, 90, 120, 180])
        return [F, d, theta], (f"A force of {F} N acts at {theta} degrees to "
                               f"the displacement over {d} m. Find the work "
                               f"done.")

    @staticmethod
    def solve(F, d, theta):
        return f"{dec(sp.Float(F) * sp.Float(d) * sp.cos(sp.rad(theta)))} J"


@relation("power", "physics", ["W", "t"], exact=False)
class Power:
    @staticmethod
    def gen(rng):
        W = rng.choice([100, 200, 300, 400, 500, 600, 800, 1000, 1200])
        t = rng.choice([2, 3, 4, 5, 6, 8, 10, 12, 15, 20])
        return [W, t], (f"{W} J of work is done in {t} s. "
                        f"Find the power.")

    @staticmethod
    def solve(W, t):
        return f"{dec(sp.Rational(W, t))} W"


@relation("momentum", "physics", ["m", "v"], exact=False)
class Momentum:
    @staticmethod
    def gen(rng):
        m = rng.choice([1, 2, 3, 4, 5, 8, 10, 12, 15, 20])
        v = rng.choice([2, 3, 4, 5, 6, 8, 10, 12, 15, 20])
        return [m, v], (f"A {m} kg object moves at {v} m/s. "
                        f"Find its momentum.")

    @staticmethod
    def solve(m, v):
        return f"{dec(m * v)} kg m/s"


@relation("newton2", "physics", ["m", "a"], exact=False)
class Newton2:
    @staticmethod
    def gen(rng):
        m = rng.choice([1, 2, 3, 4, 5, 8, 10, 12, 15, 20])
        a = rng.choice([1, 2, 3, 4, 5, 6, 8, 10])
        return [m, a], (f"A net force acts on a {m} kg mass, giving it an "
                        f"acceleration of {a} m/s^2. Find the force.")

    @staticmethod
    def solve(m, a):
        return f"{dec(m * a)} N"


@relation("circular_accel", "physics", ["v", "r"], exact=False)
class CircularAccel:
    @staticmethod
    def gen(rng):
        v = rng.choice([2, 3, 4, 5, 6, 8, 10, 12, 15, 20])
        r = rng.choice([1, 2, 3, 4, 5, 6, 8, 10, 12, 15, 20])
        return [v, r], (f"An object moves at {v} m/s in a circle of radius "
                        f"{r} m. Find its centripetal acceleration.")

    @staticmethod
    def solve(v, r):
        return f"{dec(sp.Rational(v * v, r))} m/s^2"


@relation("wave_speed", "physics", ["f", "lam"], exact=False)
class WaveSpeed:
    @staticmethod
    def gen(rng):
        f = rng.choice([2, 4, 5, 8, 10, 20, 25, 50, 100, 200])
        lam = rng.choice([1, 2, 3, 4, 5, 6, 8, 10, 12, 15])
        return [f, lam], (f"A wave has frequency {f} Hz and wavelength "
                          f"{lam} m. Find its speed.")

    @staticmethod
    def solve(f, lam):
        return f"{dec(f * lam)} m/s"


@relation("specific_heat", "physics", ["m", "c", "dT"], exact=False)
class SpecificHeat:
    @staticmethod
    def gen(rng):
        m = rng.choice([1, 2, 3, 4, 5, 8, 10, 20, 50])
        c = rng.choice([0.9, 1, 2.1, 4.18, 4.2])
        dT = rng.choice([5, 10, 15, 20, 25, 30, 40, 50])
        return [m, c, dT], (f"Find the heat needed to raise {m} kg of a "
                            f"substance with specific heat {c} J/(g C) by "
                            f"{dT} degrees.")

    @staticmethod
    def solve(m, c, dT):
        return f"{dec_exact(sp.Rational(m) * _exact(c) * sp.Rational(dT))} J"


@relation("ohm_v", "physics", ["I", "R"], exact=False)
class OhmV:
    @staticmethod
    def gen(rng):
        I = rng.choice([1, 2, 3, 4, 5, 6, 8, 10, 12])
        R = rng.choice([2, 3, 4, 5, 6, 8, 10, 12, 15, 20])
        return [I, R], (f"A current of {I} A flows through a {R} ohm "
                        f"resistor. Find the voltage across it.")

    @staticmethod
    def solve(I, R):
        return f"{dec(I * R)} V"


@relation("ohm_power", "physics", ["I", "V"], exact=False)
class OhmPower:
    @staticmethod
    def gen(rng):
        I = rng.choice([1, 2, 3, 4, 5, 6, 8, 10, 12])
        V = rng.choice([2, 3, 5, 6, 9, 12, 15, 20, 24])
        return [I, V], (f"A device draws {I} A at {V} V. "
                        f"Find the power it dissipates.")

    @staticmethod
    def solve(I, V):
        return f"{dec(I * V)} W"


@relation("free_fall", "physics", ["h", "g"], exact=False)
class FreeFall:
    @staticmethod
    def gen(rng):
        h = rng.choice([1, 2, 3, 4, 5, 6, 8, 10, 12, 15, 20, 25, 30, 45])
        g = rng.choice([9.8, 9.81, 10])
        return [h, g], (f"An object is dropped from {h} m. Find its speed "
                        f"on impact using g = {g}.")

    @staticmethod
    def solve(h, g):
        return f"{dec(sp.sqrt(2 * sp.Float(h) * sp.Float(g)))} m/s"


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


@relation("km_to_m", "units", ["km"], exact=False)
class KmToM:
    @staticmethod
    def gen(rng):
        km = rng.choice([0.5, 1, 1.5, 2, 2.5, 3, 4, 5, 8, 10, 12, 20])
        return [km], f"Convert {km} km to metres."

    @staticmethod
    def solve(km):
        return f"{dec_exact(_exact(km) * 1000)} m"


@relation("m_to_cm", "units", ["m"], exact=False)
class MToCm:
    @staticmethod
    def gen(rng):
        m = rng.choice([0.5, 1, 1.5, 2, 2.5, 3, 4, 5, 8, 10, 12, 20])
        return [m], f"Convert {m} m to centimetres."

    @staticmethod
    def solve(m):
        return f"{dec_exact(_exact(m) * 100)} cm"


@relation("celsius_to_fahrenheit", "units", ["c"], exact=False)
class CelsiusToFahrenheit:
    @staticmethod
    def gen(rng):
        c = rng.choice([-40, -20, -10, 0, 5, 10, 15, 20, 25, 30, 37, 50,
                        75, 100])
        return [c], f"Convert {c} degrees Celsius to Fahrenheit."

    @staticmethod
    def solve(c):
        return f"{dec_exact(sp.Rational(9 * c, 5) + 32)} F"


@relation("fahrenheit_to_celsius", "units", ["f"], exact=False)
class FahrenheitToCelsius:
    @staticmethod
    def gen(rng):
        f = rng.choice([-40, 0, 32, 50, 59, 68, 77, 86, 98, 100, 140, 212])
        return [f], f"Convert {f} degrees Fahrenheit to Celsius."

    @staticmethod
    def solve(f):
        return f"{dec_exact(sp.Rational(5 * (f - 32), 9))} C"


@relation("kg_to_lb", "units", ["kg"], exact=False)
class KgToLb:
    @staticmethod
    def gen(rng):
        kg = rng.choice([0.5, 1, 2, 3, 4, 5, 8, 10, 12, 20, 25, 50])
        return [kg], f"Convert {kg} kg to pounds using 1 kg = 2.205 lb."

    @staticmethod
    def solve(kg):
        return f"{dec_exact(_exact(kg) * sp.Rational(2205, 1000))} lb"


@relation("liters_to_ml", "units", ["L"], exact=False)
class LitersToMl:
    @staticmethod
    def gen(rng):
        L = rng.choice([0.25, 0.5, 1, 1.5, 2, 2.5, 3, 4, 5, 8, 10])
        return [L], f"Convert {L} L to millilitres."

    @staticmethod
    def solve(L):
        return f"{dec_exact(_exact(L) * 1000)} mL"


@relation("percent_error", "units", ["measured", "accepted"], exact=False)
class PercentError:
    @staticmethod
    def gen(rng):
        accepted = rng.choice([10, 20, 25, 40, 50, 80, 100, 120, 150, 200])
        measured = rng.randint(1, 2 * accepted)
        return [measured, accepted], (f"A measurement gives {measured} where "
                                      f"the accepted value is {accepted}. "
                                      f"Find the percent error.")

    @staticmethod
    def solve(measured, accepted):
        err = abs(measured - accepted) * 100
        return f"{dec_exact(sp.Rational(err, accepted))}%"


@relation("scientific_notation", "units", ["mantissa", "exponent"],
          exact=False)
class ScientificNotation:
    @staticmethod
    def gen(rng):
        mantissa = rng.choice([1.5, 2, 2.5, 3, 3.5, 4, 5, 6, 7.5, 8, 9])
        exponent = rng.choice([-6, -5, -4, -3, -2, 2, 3, 4, 5, 6])
        return [mantissa, exponent], (f"Write {mantissa} x 10^{exponent} in "
                                      f"decimal form.")

    @staticmethod
    def solve(mantissa, exponent):
        v = _exact(mantissa) * sp.Rational(10) ** exponent
        return dec_exact(v)


@relation("average", "units", ["a", "b", "c", "d"], exact=False)
class Average:
    @staticmethod
    def gen(rng):
        a, b, c, d = (rng.randint(1, 100) for _ in range(4))
        return [a, b, c, d], (f"Find the mean of {a}, {b}, {c} and {d}.")

    @staticmethod
    def solve(a, b, c, d):
        return dec_exact(sp.Rational(a + b + c + d, 4))
