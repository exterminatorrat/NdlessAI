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


def dec(value):
    return f"{float(value):.4g}"


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
