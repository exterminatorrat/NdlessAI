import sympy as sp

from .core import relation, poly, lin2, fmt


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
