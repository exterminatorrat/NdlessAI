import sympy as sp

from .core import relation, poly, fmt


@relation("deriv_poly3", "calculus", ["a", "b", "c", "d", "x"],
          step=dict(line="f'(x) = 3a*x^2 + 2b*x + c", expr="3*@1*@5^2 + 2*@2*@5 + @3"))
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

@relation("integr_poly3", "calculus", ["a", "b", "c", "d", "lo", "hi"],
          step=dict(line="int(a*x^3 + b*x^2 + c*x + d) from lo to hi", expr="(@1*@6^4/4 + @2*@6^3/3 + @3*@6^2/2 + @4*@6) - (@1*@5^4/4 + @2*@5^3/3 + @3*@5^2/2 + @4*@5)"))
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
