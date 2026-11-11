import sympy as sp

from .core import relation, _exact, sig_render, dec_exact


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

@relation("kg_to_lb", "units", ["kg"], exact=False,
          step=dict(line="lb = kg * 2.205", expr="@1 * 2.205"))
class KgToLb:
    @staticmethod
    def gen(rng):
        kg = rng.choice([0.5, 1, 2, 3, 4, 5, 8, 10, 12, 20, 25, 50])
        return [kg], f"Convert {kg} kg to pounds using 1 kg = 2.205 lb."

    @staticmethod
    def solve(kg):
        return f"{dec_exact(_exact(kg) * sp.Rational(2205, 1000))} lb"

@relation("liters_to_ml", "units", ["L"], exact=False,
          step=dict(line="mL = L * 1000", expr="@1 * 1000"))
class LitersToMl:
    @staticmethod
    def gen(rng):
        L = rng.choice([0.25, 0.5, 1, 1.5, 2, 2.5, 3, 4, 5, 8, 10])
        return [L], f"Convert {L} L to millilitres."

    @staticmethod
    def solve(L):
        return f"{dec_exact(_exact(L) * 1000)} mL"

@relation("percent_error", "units", ["measured", "accepted"], exact=False,
          step=dict(line="error% = |m - a| / a * 100", expr="abs(@1 - @2) / @2 * 100"))
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

@relation("average", "units", ["a", "b", "c", "d"], exact=False,
          step=dict(line="mean = (a + b + c + d)/4", expr="(@1 + @2 + @3 + @4) / 4"))
class Average:
    @staticmethod
    def gen(rng):
        a, b, c, d = (rng.randint(1, 100) for _ in range(4))
        return [a, b, c, d], (f"Find the mean of {a}, {b}, {c} and {d}.")

    @staticmethod
    def solve(a, b, c, d):
        return dec_exact(sp.Rational(a + b + c + d, 4))

@relation("sig_figs", "units", ["value", "n"], exact=True)
class SigFigs:
    @staticmethod
    def gen(rng):
        while True:
            digits = rng.randint(3, 5)
            v = rng.randint(10 ** (digits - 1), 10 ** digits - 1)
            if v % 10 == 0:
                continue
            k = rng.choice([0, 0, 1, 2, 3])
            n = rng.randint(1, digits - 1)
            value = v if k == 0 else v / 10 ** k
            text = str(value)
            if "e" in text or "E" in text:
                continue
            return [value, n], (f"Round {text} to {n} significant figures.")

    @staticmethod
    def solve(value, n):
        return sig_render(value, n)
