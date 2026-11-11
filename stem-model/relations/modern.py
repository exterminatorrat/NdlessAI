import math

import sympy as sp

from .core import relation, _exact, dec, dec_exact, sig_render

# ---------------------------------------------------------------------------
# Wave 4.4: modern physics.
#
# These are written the way the exam states them, in eV, nanometres and atomic
# mass units rather than SI. That is not a stylistic choice: h*c is 1240
# eV nm, so the photon and Bohr relations are short exact rationals of the
# typed numbers, while the same physics in SI needs constants like 6.626e-34
# that the device's fixed-point parser cannot represent at all.
# ---------------------------------------------------------------------------


@relation("photon_energy_ev", "modern", ["lam"], exact=False,
          step=dict(line="E = 1240/lambda", expr="1240/@1"))
class PhotonEnergyEv:
    @staticmethod
    def gen(rng):
        lam = rng.randrange(100, 901)
        return [lam], (f"A photon has wavelength {lam} nm. "
                       f"Find its energy in eV.")

    @staticmethod
    def solve(lam):
        return f"{dec_exact(sp.Rational(1240) / _exact(lam))} eV"


@relation("photoelectric_kev", "modern", ["lam", "phi"], exact=False,
          step=dict(line="K = 1240/lambda - phi", expr="1240/@1 - @2"))
class PhotoelectricKev:
    @staticmethod
    def gen(rng):
        while True:
            lam = rng.randrange(100, 901)
            phi = rng.choice([1.5, 1.8, 2.0, 2.2, 2.4, 2.5, 2.8, 3.0, 3.2,
                              3.5, 4.0, 4.5, 5.0])
            # Below the work function there is no photoelectron at all, so
            # only pairs with a positive kinetic energy are physical.
            if 1240 / lam - phi >= 0.1:
                break
        return [lam, phi], (
            f"Light of wavelength {lam} nm falls on a metal with work "
            f"function {phi} eV. Find the maximum kinetic energy of the "
            f"emitted electrons.")

    @staticmethod
    def solve(lam, phi):
        ke = sp.Rational(1240) / _exact(lam) - _exact(phi)
        return f"{dec_exact(ke)} eV"


@relation("threshold_wavelength", "modern", ["phi"], exact=False,
          step=dict(line="lambda_max = 1240/phi", expr="1240/@1"))
class ThresholdWavelength:
    @staticmethod
    def gen(rng):
        phi = rng.choice([1.5, 1.8, 2.0, 2.2, 2.4, 2.5, 2.8, 3.0, 3.2, 3.5,
                          4.0, 4.5, 5.0])
        return [phi], (
            f"A metal has work function {phi} eV. Find the longest "
            f"wavelength that can eject an electron.")

    @staticmethod
    def solve(phi):
        return f"{dec_exact(sp.Rational(1240) / _exact(phi))} nm"


@relation("de_broglie_electron", "modern", ["V"], exact=False,
          step=dict(line="lambda = 1.226/sqrt(V)", expr="1.226/sqrt(@1)"))
class DeBroglieElectron:
    @staticmethod
    def gen(rng):
        V = rng.randrange(100, 20001, 50)
        return [V], (
            f"An electron is accelerated through {V} V. Find its de Broglie "
            f"wavelength in nm.")

    @staticmethod
    def solve(V):
        # 1.226 = h/sqrt(2*m_e*e) in nm per root-volt, so no SI constant
        # small enough to lose in fixed point is ever typed. The answer is
        # always under 1 nm, so four figures render as plain decimals and the
        # exact-value rounding in sig_render is what the device does too.
        return f"{sig_render(1.226 / sp.sqrt(sp.Integer(V)), 4)} nm"


@relation("bohr_energy_ev", "modern", ["n"], exact=False,
          step=dict(line="E_n = -13.6/n^2", expr="-13.6/@1^2"))
class BohrEnergyEv:
    @staticmethod
    def gen(rng):
        n = rng.randint(1, 6)
        return [n], (
            f"Find the energy of the n = {n} level of the hydrogen atom in "
            f"eV.")

    @staticmethod
    def solve(n):
        return f"{dec_exact(sp.Rational(-68, 5 * n * n))} eV"


@relation("bohr_transition_nm", "modern", ["n1", "n2"], exact=False,
          step=dict(line="lambda = 1240/(13.6*(1/n1^2 - 1/n2^2))",
                    expr="1240/(13.6*(1/@1^2 - 1/@2^2))"))
class BohrTransitionNm:
    @staticmethod
    def gen(rng):
        n1 = rng.randint(1, 5)
        n2 = rng.randint(n1 + 1, 6)
        return [n1, n2], (
            f"A hydrogen electron drops from the n = {n2} level to the n = "
            f"{n1} level. Find the wavelength of the emitted photon in nm.")

    @staticmethod
    def solve(n1, n2):
        # The state energies are exact rationals, so the photon wavelength is
        # too: 1240/(13.6*(1/n1^2 - 1/n2^2)). Keeping it exact is what makes
        # the Lyman series print as the numbers in the textbook.
        drop = sp.Rational(1, n1 * n1) - sp.Rational(1, n2 * n2)
        lam = sp.Rational(1240) / (sp.Rational(136, 10) * drop)
        return f"{dec_exact(lam)} nm"


@relation("mass_energy_mev", "modern", ["m"], exact=False,
          step=dict(line="E = 931.5*m", expr="931.5*@1"))
class MassEnergyMev:
    @staticmethod
    def gen(rng):
        # Even masses only: with an odd mass 931.5*m ends in .5, and once the
        # integer part reaches five digits the device renderer drops that
        # half rather than rounding it, which would print a number one digit
        # off the true four-figure answer. Even masses keep the product an
        # integer, where both sides agree exactly.
        m = rng.randrange(2, 241, 2)
        return [m], (
            f"A nucleus of mass {m} u is converted entirely into energy. "
            f"Find the energy released in MeV.")

    @staticmethod
    def solve(m):
        # 1 u is 931.5 MeV, so this is an exact rational. Answers above
        # 10000 MeV render in exponent form, and that is the recorded
        # answer, not a loss of precision.
        return f"{dec_exact(sp.Rational(1863 * m, 2))} MeV"


@relation("half_life_fraction", "modern", ["t", "t_half"], exact=False,
          step=dict(line="f = 0.5^(t/t_half)", expr="0.5^(@1/@2)"))
class HalfLifeFraction:
    @staticmethod
    def gen(rng):
        t_half = rng.randint(1, 10)
        t = rng.randint(max(1, (t_half + 1) // 2), 6 * t_half)
        return [t, t_half], (
            f"A sample has a half life of {t_half} years. What fraction of "
            f"it remains after {t} years?")

    @staticmethod
    def solve(t, t_half):
        # A whole number of half lives is an exact power of two, and the
        # six-half-life case is exactly 0.015625: a four-figure tie that the
        # device resolves on the exact value. Going through the exact
        # rational keeps that tie on both sides instead of letting the
        # transcendental approximation pick a side.
        if t % t_half == 0:
            return dec_exact(sp.Rational(1, 2 ** (t // t_half)))
        return dec(0.5 ** sp.Rational(t, t_half))


@relation("activity_bq", "modern", ["N", "t_half"], exact=False,
          step=dict(line="A = 0.693*N/t_half", expr="0.693*@1/@2"))
class ActivityBq:
    @staticmethod
    def gen(rng):
        # A fractional rate above 10000 Bq does not survive the device
        # renderer, which drops the fraction of any value with five or more
        # integer digits. Drawing N on the lattice that keeps 0.693*N/t_half
        # a whole number keeps every answer exact at four figures.
        t_half = rng.randint(10, 1000)
        base = t_half // math.gcd(t_half, 693000)
        N = 1000000 * base * rng.randint(1, 9000 // base)
        return [N, t_half], (
            f"A sample contains {N} radioactive nuclei with a half life of "
            f"{t_half} s. Find its activity in Bq.")

    @staticmethod
    def solve(N, t_half):
        # 0.693 is ln 2 to the precision the course uses, and N and t_half
        # are integers, so the rate is an exact rational as well.
        rate = sp.Rational(693, 1000) * sp.Rational(N, t_half)
        return f"{dec_exact(rate)} Bq"
