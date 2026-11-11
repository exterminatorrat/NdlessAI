import sympy as sp

from .core import relation, _exact, dec, dec_exact

# ---------------------------------------------------------------------------
# Wave 4.4: waves and sound.
#
# Three of these are exact rationals of the decimal arguments and render
# through dec_exact, so the four significant digits match the device's
# dec4_render byte for byte. The other three go through a logarithm, an
# exponential or a square root: there the runtime evaluates an integer series,
# the double is the better approximation of the true value, and dec is what
# agrees with it.
#
# The argument sets are the ones a student is handed, and they are also what
# keeps every argument a plain decimal string. 1e-12 W/m^2 cannot be typed on
# the device at all, which is why sound_level starts at 0.0001 and
# sound_intensity takes the level rather than the intensity.
# ---------------------------------------------------------------------------


@relation("sound_level", "physics", ["I"], exact=False,
          step=dict(line="L = 10*log10(I/1e-12)", expr="10*log10(@1/1e-12)"))
class SoundLevel:
    @staticmethod
    def gen(rng):
        I = rng.choice([0.0001, 0.0002, 0.0005, 0.001, 0.002, 0.005, 0.01,
                        0.02, 0.05, 0.1, 0.2, 0.5, 1, 2, 5, 10])
        return [I], (f"A sound wave has an intensity of {I} W/m^2. Find its "
                     f"sound level in decibels.")

    @staticmethod
    def solve(I):
        return f"{dec(10 * sp.log(sp.Float(I) / sp.Float('1e-12'), 10))} dB"


@relation("sound_intensity", "physics", ["L"], exact=False,
          step=dict(line="I = 1e-12*10^(L/10)", expr="1e-12*10^(@1/10)"))
class SoundIntensity:
    @staticmethod
    def gen(rng):
        L = rng.choice([80, 85, 90, 95, 100, 105, 110, 115, 120])
        return [L], (f"A sound has a sound level of {L} dB. Find its "
                     f"intensity in W/m^2.")

    @staticmethod
    def solve(L):
        I = sp.Float(10) ** (sp.Float(L) / 10) * sp.Float("1e-12")
        return f"{dec(I)} W/m^2"


@relation("doppler_freq", "physics", ["f", "v", "vs"], exact=False,
          step=dict(line="f' = f*v/(v - vs)", expr="@1*@2/(@2 - @3)"))
class DopplerFreq:
    @staticmethod
    def gen(rng):
        f = rng.randrange(200, 1001, 10)
        v = rng.choice([330, 340, 343, 344])
        vs = rng.randint(5, v - 20)
        return [f, v, vs], (f"A siren emits a {f} Hz tone. Its source moves "
                            f"toward a stationary listener at {vs} m/s, where "
                            f"sound travels at {v} m/s. Find the frequency the "
                            f"listener hears.")

    @staticmethod
    def solve(f, v, vs):
        shifted = _exact(f) * _exact(v) / (_exact(v) - _exact(vs))
        return f"{dec_exact(shifted)} Hz"


@relation("beat_freq", "physics", ["f1", "f2"], exact=False,
          step=dict(line="f_b = |f1 - f2|", expr="abs(@1 - @2)"))
class BeatFreq:
    @staticmethod
    def gen(rng):
        f1 = rng.randrange(200, 985)
        f2 = f1 + rng.randint(1, 15)
        return [f1, f2], (f"Two tuning forks sound together at {f1} Hz and "
                          f"{f2} Hz. Find the beat frequency.")

    @staticmethod
    def solve(f1, f2):
        return f"{dec(abs(f1 - f2))} Hz"


@relation("standing_wave_freq", "physics", ["n", "v", "L"], exact=False,
          step=dict(line="f_n = n*v/(2*L)", expr="@1*@2/(2*@3)"))
class StandingWaveFreq:
    @staticmethod
    def gen(rng):
        n = rng.randint(1, 5)
        v = rng.choice([330, 340, 343, 344])
        L = rng.choice([0.5, 1, 1.5, 2])
        return [n, v, L], (f"A string {L} m long is fixed at both ends and "
                           f"carries waves at {v} m/s. Find the frequency of "
                           f"harmonic n = {n}.")

    @staticmethod
    def solve(n, v, L):
        return f"{dec_exact(_exact(n) * _exact(v) / (2 * _exact(L)))} Hz"


@relation("string_freq", "physics", ["T", "mu", "L"], exact=False,
          step=dict(line="f = sqrt(T/mu)/(2*L)", expr="sqrt(@1/@2)/(2*@3)"))
class StringFreq:
    @staticmethod
    def gen(rng):
        T = rng.choice([50, 75, 100, 150, 200, 250, 300, 400, 500])
        mu = rng.choice([0.001, 0.002, 0.005, 0.01])
        L = rng.choice([0.4, 0.5, 0.6, 0.8, 1, 1.2])
        return [T, mu, L], (f"A guitar string {L} m long has a linear density "
                            f"of {mu} kg/m and is under {T} N of tension. Find "
                            f"its fundamental frequency.")

    @staticmethod
    def solve(T, mu, L):
        f = sp.sqrt(sp.Float(T) / sp.Float(mu)) / (2 * sp.Float(L))
        return f"{dec(f)} Hz"


@relation("double_slit_fringe", "physics", ["m", "lam", "L", "d"], exact=False,
          step=dict(line="y = m*lam*L/(1000*d)", expr="@1*@2*@3/(1000*@4)"))
class DoubleSlitFringe:
    @staticmethod
    def gen(rng):
        m = rng.randint(1, 3)
        lam = rng.randrange(400, 701, 10)
        L = rng.choice([0.5, 1, 1.5, 2, 2.5, 3])
        d = rng.choice([0.1, 0.2, 0.25, 0.5, 0.75, 1])
        return [m, lam, L, d], (f"In a double-slit experiment the slits are "
                                f"{d} mm apart, the screen is {L} m from them "
                                f"and the light has wavelength {lam} nm. Find "
                                f"the distance in mm from the centre to the "
                                f"bright fringe of order m = {m}.")

    @staticmethod
    def solve(m, lam, L, d):
        y = _exact(m) * _exact(lam) * _exact(L) / (1000 * _exact(d))
        return f"{dec_exact(y)} mm"
