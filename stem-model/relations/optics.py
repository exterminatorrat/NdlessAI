"""Wave 4.4: geometric optics.

Angles are in degrees on both sides, matching the questions and the rest of the
catalogue; the answer carries no unit, the way the trigonometry relations state
an angle in their own words. Every index is a plain decimal an exam would give,
because an argument has to survive the device's number parser verbatim.
"""

import math

import sympy as sp

from .core import relation, _exact, dec, dec_exact

# Index pairs and an incident angle the boundary will actually refract. Total
# internal reflection is not a refraction question, and an equation with no
# solution is not a relation.
_SNELL = [
    (1.0, 45, 1.5), (1.0, 30, 2.42), (1.33, 30, 1.0), (1.5, 35, 1.0),
    (1.5, 40, 1.33), (1.6, 50, 1.5), (1.33, 60, 1.6), (1.0, 20, 1.33),
    (1.5, 25, 2.0), (1.6, 30, 1.33), (1.33, 45, 1.5), (1.0, 50, 1.6),
]

_TOTAL = [(1.5, 1.0), (1.6, 1.33), (2.0, 1.0), (2.42, 1.33), (1.5, 1.33),
          (1.6, 1.0), (2.0, 1.5), (2.42, 1.6)]


@relation("snell_angle", "optics", ["n1", "th1", "n2"], exact=False,
          step=dict(line="n2*sin(th2) = n1*sin(th1)",
                    expr="asin(@1*sin(@2)/@3)"))
class SnellAngle:
    @staticmethod
    def gen(rng):
        n1, th1, n2 = rng.choice(_SNELL)
        return [n1, th1, n2], (
            f"Light travelling in a medium of refractive index {n1} reaches a "
            f"boundary with a medium of index {n2} at {th1} degrees to the "
            f"normal. Find the angle of refraction, in degrees.")

    @staticmethod
    def solve(n1, th1, n2):
        x = float(n1) * math.sin(math.radians(th1)) / float(n2)
        return dec(sp.Float(math.degrees(math.asin(x))))


@relation("critical_angle", "optics", ["n1", "n2"], exact=False,
          step=dict(line="sin(thc) = n2/n1", expr="asin(@2/@1)"))
class CriticalAngle:
    @staticmethod
    def gen(rng):
        n1, n2 = rng.choice(_TOTAL)
        return [n1, n2], (
            f"Light in a medium of refractive index {n1} meets a boundary with "
            f"a medium of index {n2}. Find the critical angle, in degrees.")

    @staticmethod
    def solve(n1, n2):
        return dec(sp.Float(math.degrees(math.asin(float(n2) / float(n1)))))


@relation("lens_image_distance", "optics", ["f", "do"], exact=False,
          step=dict(line="1/f = 1/do + 1/di", expr="1/(1/@1 - 1/@2)"))
class LensImageDistance:
    @staticmethod
    def gen(rng):
        f = rng.choice([5, 8, 10, 12, 15, 20, 25])
        do = rng.choice([30, 35, 40, 45, 50, 60, 75, 90])
        if do == f:
            do += 15
        return [f, do], (
            f"An object stands {do} cm in front of a converging lens of focal "
            f"length {f} cm. At what distance from the lens does the image "
            f"form, in cm?")

    @staticmethod
    def solve(f, do):
        return f"{dec_exact(sp.Rational(f * do, do - f))} cm"


@relation("lens_magnification", "optics", ["di", "do"], exact=False,
          step=dict(line="m = -di/do", expr="-@1/@2"))
class LensMagnification:
    @staticmethod
    def gen(rng):
        do = rng.choice([10, 15, 20, 25, 30, 40, 50])
        di = rng.choice([5, 8, 10, 12, 15, 20, 25, 30, 45, 60])
        return [di, do], (
            f"An image forms {di} cm from a lens while the object is {do} cm "
            f"in front of it. Find the magnification.")

    @staticmethod
    def solve(di, do):
        return dec_exact(sp.Rational(-di, do))


@relation("mirror_image_distance", "optics", ["f", "do"], exact=False,
          step=dict(line="1/f = 1/do + 1/di", expr="1/(1/@1 - 1/@2)"))
class MirrorImageDistance:
    @staticmethod
    def gen(rng):
        f = rng.choice([6, 9, 10, 12, 15, 18, 24, 30])
        do = rng.choice([20, 25, 36, 40, 45, 60, 80])
        if do == f:
            do += 12
        return [f, do], (
            f"An object is {do} cm in front of a concave mirror of focal "
            f"length {f} cm. Find the image distance, in cm.")

    @staticmethod
    def solve(f, do):
        return f"{dec_exact(sp.Rational(f * do, do - f))} cm"


@relation("index_from_speed", "optics", ["v"], exact=False,
          step=dict(line="n = c/v", expr="3/@1"))
class IndexFromSpeed:
    """The speed is stated in units of 1e8 m/s.

    c = 3.0e8 m/s does not fit the device's number parser as an argument, and a
    student reading "1.9 x 10^8 m/s" does the same division the relation does.
    """

    @staticmethod
    def gen(rng):
        v = rng.choice([1.2, 1.24, 1.3, 1.4, 1.5, 1.6, 1.75, 1.8, 1.9, 2.0,
                        2.1, 2.2, 2.4])
        return [v], (f"Light travels through a material at {v} x 10^8 m/s. "
                     f"Find the material's refractive index.")

    @staticmethod
    def solve(v):
        return dec_exact(sp.Rational(3) / sp.Rational(str(v)))
