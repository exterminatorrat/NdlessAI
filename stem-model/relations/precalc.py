import sympy as sp

from .core import relation, poly, lin2, fmt, dec_exact


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
        q = f"Solve the system {lin2(a1, b1, c1)} and {lin2(a2, b2, c2)}."
        return [a1, b1, c1, a2, b2, c2], q

    @staticmethod
    def solve(a1, b1, c1, a2, b2, c2):
        x, y = sp.symbols("x y")
        sol = sp.solve([sp.Eq(a1 * x + b1 * y, c1), sp.Eq(a2 * x + b2 * y, c2)], [x, y])
        return f"x={fmt(sol[x])}, y={fmt(sol[y])}"

@relation("exp_solve", "precalc", ["base", "k"],
          step=dict(line="x = ln(k)/ln(b)", expr="ln(@2)/ln(@1)"))
class ExpSolve:
    @staticmethod
    def gen(rng):
        base = rng.choice([2, 3, 5, 10])
        k = base ** rng.randint(2, 4)
        return [base, k], f"Solve {base}^x = {k} for x."

    @staticmethod
    def solve(base, k):
        return fmt(sp.log(k) / sp.log(base))


@relation("average_rate_of_change", "precalc", ["fa", "fb", "a", "b"], exact=False,
          step=dict(line="rate = (f(b) - f(a)) / (b - a)",
                    expr="(@2 - @1) / (@4 - @3)"))
class AverageRateOfChange:
    @staticmethod
    def gen(rng):
        a = rng.randint(1, 6)
        b = a + rng.choice([2, 3, 4, 5])
        slope = rng.choice([-3, -2, -1, 2, 3, 4, 5])
        fa = rng.choice([-4, -2, 0, 1, 3, 6, 9])
        fb = fa + slope * (b - a)
        return [fa, fb, a, b], (f"A table for a function gives f({a}) = {fa} "
                                f"and f({b}) = {fb}. Find the average rate of "
                                f"change over that interval.")

    @staticmethod
    def solve(fa, fb, a, b):
        return dec_exact(sp.Rational(fb - fa, b - a))

@relation("vector_add", "precalc", ["ax", "ay", "bx", "by"], exact=False,
          step=dict(line="(ax + bx, ay + by)", expr="(@1 + @3, @2 + @4)"))
class VectorAdd:
    @staticmethod
    def gen(rng):
        ax, ay = rng.randint(-6, 9), rng.randint(-6, 9)
        bx, by = rng.randint(-6, 9), rng.randint(-6, 9)
        return [ax, ay, bx, by], (f"Add the vectors ({ax}, {ay}) and "
                                  f"({bx}, {by}). Give the components.")

    @staticmethod
    def solve(ax, ay, bx, by):
        return f"x={dec_exact(sp.Rational(ax + bx))}, y={dec_exact(sp.Rational(ay + by))}"

@relation("vector_dot", "precalc", ["ax", "ay", "bx", "by"], exact=False,
          step=dict(line="a . b = ax*bx + ay*by", expr="@1 * @3 + @2 * @4"))
class VectorDot:
    @staticmethod
    def gen(rng):
        ax, ay = rng.randint(-6, 9), rng.randint(-6, 9)
        bx, by = rng.randint(-6, 9), rng.randint(-6, 9)
        return [ax, ay, bx, by], (f"Find the dot product of ({ax}, {ay}) and "
                                  f"({bx}, {by}).")

    @staticmethod
    def solve(ax, ay, bx, by):
        return dec_exact(sp.Rational(ax * bx + ay * by))

@relation("vector_scale", "precalc", ["ax", "ay", "k"], exact=False,
          step=dict(line="(k*ax, k*ay)", expr="(@3 * @1, @3 * @2)"))
class VectorScale:
    @staticmethod
    def gen(rng):
        ax, ay = rng.randint(-6, 9), rng.randint(-6, 9)
        k = rng.choice([-3, -2, -1, 2, 3, 4, 5])
        return [ax, ay, k], (f"Scale the vector ({ax}, {ay}) by {k}. Give the "
                             f"components.")

    @staticmethod
    def solve(ax, ay, k):
        return f"x={dec_exact(sp.Rational(k * ax))}, y={dec_exact(sp.Rational(k * ay))}"
