import sympy as sp

from .core import relation, _exact, sig_render, dec_exact


@relation("km_to_m", "units", ["km"], exact=False,
          step=dict(line="m = km*1000", expr="@1*1000"))
class KmToM:
    @staticmethod
    def gen(rng):
        km = rng.choice([0.5, 1, 1.5, 2, 2.5, 3, 4, 5, 8, 10, 12, 20])
        return [km], f"Convert {km} km to metres."

    @staticmethod
    def solve(km):
        return f"{dec_exact(_exact(km) * 1000)} m"

@relation("m_to_cm", "units", ["m"], exact=False,
          step=dict(line="cm = m*100", expr="@1*100"))
class MToCm:
    @staticmethod
    def gen(rng):
        m = rng.choice([0.5, 1, 1.5, 2, 2.5, 3, 4, 5, 8, 10, 12, 20])
        return [m], f"Convert {m} m to centimetres."

    @staticmethod
    def solve(m):
        return f"{dec_exact(_exact(m) * 100)} cm"

@relation("celsius_to_fahrenheit", "units", ["c"], exact=False,
          step=dict(line="F = C*9/5 + 32", expr="@1*9/5 + 32"))
class CelsiusToFahrenheit:
    @staticmethod
    def gen(rng):
        c = rng.choice([-40, -20, -10, 0, 5, 10, 15, 20, 25, 30, 37, 50,
                        75, 100])
        return [c], f"Convert {c} degrees Celsius to Fahrenheit."

    @staticmethod
    def solve(c):
        return f"{dec_exact(sp.Rational(9 * c, 5) + 32)} F"

@relation("fahrenheit_to_celsius", "units", ["f"], exact=False,
          step=dict(line="C = (F - 32)*5/9", expr="(@1 - 32)*5/9"))
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


@relation("percent_change", "units", ["original", "new"], exact=False,
          step=dict(line="change% = (new - original) / original * 100",
                    expr="(@2 - @1) / @1 * 100"))
class PercentChange:
    @staticmethod
    def gen(rng):
        original = rng.choice([40, 60, 80, 100, 120, 200, 240])
        pct = rng.choice([-50, -25, -20, -10, 10, 20, 25, 50])
        new = original + original * pct // 100
        return [original, new], (f"A price changes from {original} to {new}. "
                                 f"Find the percent change.")

    @staticmethod
    def solve(original, new):
        return f"{dec_exact(sp.Rational(new - original, original) * 100)}%"

@relation("percent_of", "units", ["percent", "whole"], exact=False,
          step=dict(line="part = percent/100 * whole", expr="@1 / 100 * @2"))
class PercentOf:
    @staticmethod
    def gen(rng):
        percent = rng.choice([5, 10, 15, 20, 25, 40, 60, 75])
        whole = rng.choice([40, 60, 80, 120, 200, 240, 400])
        return [percent, whole], (f"What is {percent}% of {whole}?")

    @staticmethod
    def solve(percent, whole):
        return dec_exact(sp.Rational(percent, 100) * whole)

@relation("ratio_share", "units", ["total", "a", "b"], exact=False,
          step=dict(line="first share = total * a / (a + b)",
                    expr="@1 * @2 / (@2 + @3)"))
class RatioShare:
    @staticmethod
    def gen(rng):
        a = rng.randint(1, 5)
        b = rng.randint(1, 5)
        total = rng.choice([30, 45, 60, 90, 120, 150]) * (a + b)
        return [total, a, b], (f"An amount of {total} is shared in the ratio "
                               f"{a}:{b}. How much is the first share?")

    @staticmethod
    def solve(total, a, b):
        return dec_exact(sp.Rational(total * a, a + b))

@relation("average_speed", "units", ["distance", "time"], exact=False,
          step=dict(line="v = d / t", expr="@1 / @2"))
class AverageSpeed:
    @staticmethod
    def gen(rng):
        distance = rng.choice([60, 90, 120, 150, 200, 240, 300])
        time = rng.choice([2, 3, 4, 5, 6, 8])
        return [distance, time], (f"A car covers {distance} km in {time} hours. "
                                  f"Find its average speed in km/h.")

    @staticmethod
    def solve(distance, time):
        return f"{dec_exact(sp.Rational(distance, time))} km/h"

@relation("simple_interest", "units", ["principal", "rate", "years"], exact=False,
          step=dict(line="I = P * r/100 * t", expr="@1 * @2 / 100 * @3"))
class SimpleInterest:
    @staticmethod
    def gen(rng):
        principal = rng.choice([500, 1000, 1500, 2000, 2500, 4000])
        rate = rng.choice([2, 3, 4, 5, 6, 8])
        years = rng.randint(1, 5)
        return [principal, rate, years], (
            f"Find the simple interest on {principal} at {rate}% per year for "
            f"{years} years.")

    @staticmethod
    def solve(principal, rate, years):
        return dec_exact(sp.Rational(principal * rate * years, 100))


@relation("rectangle_area", "units", ["width", "height"], exact=False,
          step=dict(line="A = w * h", expr="@1 * @2"))
class RectangleArea:
    @staticmethod
    def gen(rng):
        w = rng.choice([3, 4, 5, 6, 8, 12, 15])
        h = rng.choice([2, 3, 5, 7, 9, 11, 14])
        return [w, h], (f"A rectangle is {w} cm wide and {h} cm tall. "
                        f"Find its area.")

    @staticmethod
    def solve(width, height):
        return dec_exact(sp.Rational(width) * height)

@relation("triangle_area", "units", ["base", "height"], exact=False,
          step=dict(line="A = base * height / 2", expr="@1 * @2 / 2"))
class TriangleArea:
    @staticmethod
    def gen(rng):
        base = rng.choice([4, 6, 8, 10, 12, 14, 16])
        height = rng.choice([3, 5, 7, 9, 11, 13, 15])
        return [base, height], (f"A triangle has base {base} m and height "
                                f"{height} m. Find its area.")

    @staticmethod
    def solve(base, height):
        return dec_exact(sp.Rational(base * height, 2))

@relation("volume_prism", "units", ["length", "width", "height"], exact=False,
          step=dict(line="V = l * w * h", expr="@1 * @2 * @3"))
class VolumePrism:
    @staticmethod
    def gen(rng):
        l = rng.choice([2, 3, 4, 5, 6, 8])
        w = rng.choice([2, 3, 4, 5, 7])
        h = rng.choice([3, 4, 6, 9, 10])
        return [l, w, h], (f"A box measures {l} cm by {w} cm by {h} cm. "
                           f"Find its volume.")

    @staticmethod
    def solve(length, width, height):
        return dec_exact(sp.Rational(length * width * height))

@relation("combined_rate", "units", ["a", "b"], exact=False,
          step=dict(line="t = a*b / (a + b)", expr="@1 * @2 / (@1 + @2)"))
class CombinedRate:
    @staticmethod
    def gen(rng):
        a = rng.choice([2, 3, 4, 5, 6, 8, 12])
        b = rng.choice([3, 4, 5, 6, 8, 10, 12])
        while b == a:
            b = rng.choice([3, 4, 5, 6, 8, 10, 12])
        return [a, b], (f"One pipe fills a tank in {a} hours and a second "
                        f"fills it in {b} hours. How many hours do they take "
                        f"together?")

    @staticmethod
    def solve(a, b):
        return dec_exact(sp.Rational(a * b, a + b))

@relation("min_of", "units", ["a", "b"], exact=True)
class MinOf:
    """The smaller of two numbers, which is what "limiting" decides.

    A chain's only control flow is data flow: a step cannot branch on which
    reactant runs out. It can compare, though, if the comparison is a call —
    and the stoichiometry rows that ask for the maximum product are exactly
    `min(n_a * ratio_a, n_b * ratio_b)`. No step template: a minimum is not
    arithmetic over the arguments, so there is no substitution to show.
    """

    @staticmethod
    def gen(rng):
        a = rng.choice([1, 2, 3, 4, 5, 6, 8, 10, 12, 15, 18, 20])
        b = rng.choice([1, 2, 3, 4, 5, 6, 8, 10, 12, 15, 18, 20])
        while b == a:
            b = rng.choice([1, 2, 3, 4, 5, 6, 8, 10, 12, 15, 18, 20])
        return [a, b], (f"Two possible amounts are {a} and {b}. Which is the "
                        f"smaller?")

    @staticmethod
    def solve(a, b):
        return dec_exact(min(_exact(a), _exact(b)))


@relation("probability_single", "units", ["favourable", "total"], exact=False,
          step=dict(line="P = favourable / total", expr="@1 / @2"))
class ProbabilitySingle:
    @staticmethod
    def gen(rng):
        total = rng.choice([5, 8, 10, 12, 15, 20, 25])
        favourable = rng.randint(1, total - 1)
        return [favourable, total], (f"A bag holds {total} counters, "
                                     f"{favourable} of them red. One counter "
                                     f"is drawn at random. Find the probability "
                                     f"it is red, as a decimal.")

    @staticmethod
    def solve(favourable, total):
        return dec_exact(sp.Rational(favourable, total))
