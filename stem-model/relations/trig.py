import sympy as sp

from .core import relation, poly, amp, fmt, dec


@relation("sin_eval", "trig", ["a"], exact=False,
          step=dict(line="sin(a)", expr="sin(@1)"))
class SinEval:
    @staticmethod
    def gen(rng):
        a = rng.choice([0, 5, 10, 12, 15, 20, 25, 30, 35, 37, 40, 45,
                        50, 53, 55, 60, 65, 70, 75, 80, 85, 90])
        return [a], f"What is sin({a})?"

    @staticmethod
    def solve(a):
        return dec(sp.sin(sp.rad(a)))

@relation("cos_eval", "trig", ["a"], exact=False,
          step=dict(line="cos(a)", expr="cos(@1)"))
class CosEval:
    @staticmethod
    def gen(rng):
        a = rng.choice([0, 5, 10, 12, 15, 20, 25, 30, 35, 37, 40, 45,
                        50, 53, 55, 60, 65, 70, 75, 80, 85, 90])
        return [a], f"What is cos({a})?"

    @staticmethod
    def solve(a):
        return dec(sp.cos(sp.rad(a)))

@relation("tan_eval", "trig", ["a"], exact=False,
          step=dict(line="tan(a)", expr="tan(@1)"))
class TanEval:
    @staticmethod
    def gen(rng):
        a = rng.choice([0, 5, 10, 12, 15, 20, 25, 30, 35, 37, 40, 45,
                        50, 53, 55, 60, 65, 70, 75, 80, 85])
        return [a], f"What is tan({a})?"

    @staticmethod
    def solve(a):
        return dec(sp.tan(sp.rad(a)))

@relation("pythagoras", "trig", ["a", "b"], exact=False,
          step=dict(line="c = sqrt(a^2 + b^2)", expr="sqrt(@1^2 + @2^2)"))
class Pythagoras:
    @staticmethod
    def gen(rng):
        a, b = rng.randint(3, 20), rng.randint(3, 20)
        return [a, b], (f"A right triangle has legs {a} and {b}. "
                        f"Find the hypotenuse.")

    @staticmethod
    def solve(a, b):
        return dec(sp.sqrt(a ** 2 + b ** 2))

@relation("law_sines", "trig", ["a", "A", "B"], exact=False,
          step=dict(line="b = a*sin(B)/sin(A)", expr="@1*sin(@3)/sin(@2)"))
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

@relation("law_cosines", "trig", ["a", "b", "C"], exact=False,
          step=dict(line="c = sqrt(a^2 + b^2 - 2ab*cos(C))", expr="sqrt(@1^2 + @2^2 - 2*@1*@2*cos(@3))"))
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

@relation("arc_length", "trig", ["r", "theta"], exact=False,
          step=dict(line="s = r*theta*pi/180", expr="@1*@2*pi/180"))
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

@relation("sinusoid", "trig", ["A", "B", "C"], exact=True)
class Sinusoid:
    @staticmethod
    def gen(rng):
        A = rng.choice([-5, -4, -3, -2, -1, 1, 2, 3, 4, 5])
        B = rng.choice([1, 2, 3, 4, 6])
        C = rng.choice([-90, -60, -45, -30, 0, 30, 45, 60, 90])
        return [A, B, C], (f"For y = {amp(A)}sin({poly([B, C])}), find the amplitude, "
                           f"period and phase shift in degrees.")

    @staticmethod
    def solve(A, B, C):
        amp = abs(A)
        period = sp.Rational(360, abs(B))
        shift = sp.Rational(-C, B)
        return f"amp={fmt(amp)}, period={fmt(period)}, shift={fmt(shift)}"

@relation("right_triangle", "trig", ["opp", "adj"], exact=False)
class RightTriangle:
    @staticmethod
    def gen(rng):
        opp = rng.randint(1, 20)
        adj = rng.randint(1, 20)
        return [opp, adj], (f"A right triangle has opposite side {opp} and "
                            f"adjacent side {adj}. Find the angle and the "
                            f"hypotenuse.")

    @staticmethod
    def solve(opp, adj):
        angle = sp.atan2(opp, adj) * 180 / sp.pi
        hyp = sp.sqrt(opp ** 2 + adj ** 2)
        return f"angle={dec(angle)}, hyp={dec(hyp)}"
