import sympy as sp

from .core import relation, poly, fmt


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
        return [a, b, c], f"Solve |{poly([a, b])}| = {c} for x."

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
        return [a, b, c], f"Solve {poly([a, b])} < {c} for x."

    @staticmethod
    def solve(a, b, c):
        bound = sp.Rational(c - b, a)
        op = "<" if a > 0 else ">"
        return f"x {op} {fmt(bound)}"

@relation("complete_square", "algebra", ["a", "b", "c"], exact=True)
class CompleteSquare:
    @staticmethod
    def gen(rng):
        a = rng.choice([1, 1, 1, 2, 3])
        b = rng.randint(-10, 10)
        c = rng.randint(-10, 10)
        return [a, b, c], (f"Complete the square for {poly([a, b, c])} and "
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
        return [a, b, c], f"Factor {poly([a, b, c])}."

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

@relation("cubic_solve", "algebra", ["a", "b", "c", "d"], exact=True)
class CubicSolve:
    @staticmethod
    def gen(rng):
        a = rng.choice([1, 1, 1, 2])
        r1, r2, r3 = (rng.randint(-5, 5) for _ in range(3))
        b = -a * (r1 + r2 + r3)
        c = a * (r1 * r2 + r1 * r3 + r2 * r3)
        d = -a * r1 * r2 * r3
        return [a, b, c, d], f"Solve {poly([a, b, c, d])} = 0 for x."

    @staticmethod
    def solve(a, b, c, d):
        roots = sp.roots(sp.Poly([a, b, c, d], sp.Symbol("x")))
        vals = []
        for r, mult in roots.items():
            vals.extend([r] * mult)
        vals.sort()
        return ", ".join(f"x={fmt(v)}" for v in vals)
