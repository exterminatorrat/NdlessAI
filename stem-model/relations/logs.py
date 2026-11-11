import sympy as sp

from .core import relation, dec, dec_exact


@relation("log_eval", "logs", ["base", "x"], exact=False,
          step=dict(line="log_b(x) = ln(x)/ln(b)", expr="ln(@2)/ln(@1)"))
class LogEval:
    @staticmethod
    def gen(rng):
        base = rng.choice([2, 3, 4, 5])
        x = rng.choice([2, 3, 4, 5, 6, 8, 9, 10, 12, 16, 20, 25, 27,
                        32, 36, 49, 64, 81, 100, 125, 128, 243, 1000])
        return [base, x], f"What is log base {base} of {x}?"

    @staticmethod
    def solve(base, x):
        return dec(sp.log(x, base))

@relation("ln_eval", "logs", ["x"], exact=False,
          step=dict(line="ln(x)", expr="ln(@1)"))
class LnEval:
    @staticmethod
    def gen(rng):
        x = rng.choice([2, 3, 4, 5, 6, 8, 10, 12, 15, 20, 25, 30, 50, 100])
        return [x], f"What is ln({x})?"

    @staticmethod
    def solve(x):
        return dec(sp.log(x))

@relation("log10_eval", "logs", ["x"], exact=False,
          step=dict(line="log10(x)", expr="log10(@1)"))
class Log10Eval:
    @staticmethod
    def gen(rng):
        x = rng.choice([2, 3, 4, 5, 6, 8, 10, 12, 15, 20, 25, 30, 50,
                        100, 1000])
        return [x], f"What is log({x})?"

    @staticmethod
    def solve(x):
        return dec(sp.log(x, 10))

@relation("compound_interest", "logs", ["P", "r", "n", "t"], exact=False,
          step=dict(line="A = P*(1 + r/n)^(n*t)", expr="@1*(1 + @2/100/@3)^(@3*@4)"))
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

@relation("continuous_growth", "logs", ["P", "r", "t"], exact=False,
          step=dict(line="A = P*e^(r*t)", expr="@1*exp(@2/100*@3)"))
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

@relation("half_life", "logs", ["initial", "hl", "t"], exact=False,
          step=dict(line="N = N0*(1/2)^(t/hl)", expr="@1*(1/2)^(@3/@2)"))
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

@relation("doubling_time", "logs", ["r"], exact=False,
          step=dict(line="t = ln(2)/ln(1 + r/100)", expr="ln(2)/ln(1 + @1/100)"))
class DoublingTime:
    @staticmethod
    def gen(rng):
        r = rng.choice([2, 3, 4, 5, 6, 7, 8, 10, 12])
        return [r], (f"An investment grows at {r}% per year. "
                     f"How many years does it take to double?")

    @staticmethod
    def solve(r):
        return dec(sp.log(2) / sp.log(1 + sp.Float(r) / 100))
