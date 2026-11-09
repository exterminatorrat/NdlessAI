import sympy as sp

from .core import relation, _exact, dec, dec_exact


@relation("kinematic_v", "physics", ["v0", "a", "t"], exact=False)
class KinematicV:
    @staticmethod
    def gen(rng):
        v0, a, t = rng.randint(0, 20), rng.randint(-5, 8), rng.randint(1, 10)
        q = (f"A body starts at {v0} m/s with constant acceleration {a} m/s^2. "
             f"What is its velocity after {t} s?")
        return [v0, a, t], q

    @staticmethod
    def solve(v0, a, t):
        return f"{dec(sp.Float(v0 + a * t))} m/s"

@relation("proj_range", "physics", ["v0", "theta", "g"], exact=False)
class ProjRange:
    @staticmethod
    def gen(rng):
        v0 = rng.randrange(5, 40)
        theta = rng.choice([15, 20, 25, 30, 35, 40, 45, 50, 55, 60])
        g = rng.choice([9.8, 9.81, 10])
        q = (f"A projectile is launched at {v0} m/s at {theta} degrees above the "
             f"horizontal. What horizontal range does it cover with g = {g} m/s^2?")
        return [v0, theta, g], q

    @staticmethod
    def solve(v0, theta, g):
        rad = sp.rad(sp.Integer(theta))
        r = sp.Integer(v0) ** 2 * sp.sin(2 * rad) / sp.Float(g)
        return f"{dec(r)} m"

@relation("ohm_i", "physics", ["V", "R"], exact=False)
class OhmI:
    @staticmethod
    def gen(rng):
        V, R = rng.randint(1, 24), rng.choice([2, 3, 4, 5, 6, 8, 10, 12])
        return [V, R], f"A {R} ohm resistor has {V} V across it. What current flows?"

    @staticmethod
    def solve(V, R):
        return f"{dec(sp.Float(V) / sp.Float(R))} A"

@relation("kinetic_energy", "physics", ["m", "v"], exact=False)
class KineticEnergy:
    @staticmethod
    def gen(rng):
        m = rng.choice([1, 2, 3, 4, 5, 8, 10, 12, 15, 20])
        v = rng.choice([2, 3, 4, 5, 6, 8, 10, 12, 15, 20])
        return [m, v], (f"A {m} kg object moves at {v} m/s. "
                        f"Find its kinetic energy.")

    @staticmethod
    def solve(m, v):
        return f"{dec(sp.Rational(m * v * v, 2))} J"

@relation("potential_energy", "physics", ["m", "h", "g"], exact=False)
class PotentialEnergy:
    @staticmethod
    def gen(rng):
        m = rng.choice([1, 2, 3, 4, 5, 8, 10, 12, 15, 20])
        h = rng.choice([1, 2, 3, 4, 5, 6, 8, 10, 12, 15, 20])
        g = rng.choice([9.8, 9.81, 10])
        return [m, h, g], (f"A {m} kg mass is raised {h} m. Find its "
                           f"gravitational potential energy using g = {g}.")

    @staticmethod
    def solve(m, h, g):
        return f"{dec_exact(sp.Rational(m) * sp.Rational(h) * _exact(g))} J"

@relation("work", "physics", ["F", "d", "theta"], exact=False)
class Work:
    @staticmethod
    def gen(rng):
        F = rng.choice([10, 20, 25, 30, 40, 50, 60, 80, 100])
        d = rng.choice([2, 3, 4, 5, 6, 8, 10, 12, 15, 20])
        theta = rng.choice([0, 30, 45, 60, 90, 120, 180])
        return [F, d, theta], (f"A force of {F} N acts at {theta} degrees to "
                               f"the displacement over {d} m. Find the work "
                               f"done.")

    @staticmethod
    def solve(F, d, theta):
        return f"{dec(sp.Float(F) * sp.Float(d) * sp.cos(sp.rad(theta)))} J"

@relation("power", "physics", ["W", "t"], exact=False)
class Power:
    @staticmethod
    def gen(rng):
        W = rng.choice([100, 200, 300, 400, 500, 600, 800, 1000, 1200])
        t = rng.choice([2, 3, 4, 5, 6, 8, 10, 12, 15, 20])
        return [W, t], (f"{W} J of work is done in {t} s. "
                        f"Find the power.")

    @staticmethod
    def solve(W, t):
        return f"{dec(sp.Rational(W, t))} W"

@relation("momentum", "physics", ["m", "v"], exact=False)
class Momentum:
    @staticmethod
    def gen(rng):
        m = rng.choice([1, 2, 3, 4, 5, 8, 10, 12, 15, 20])
        v = rng.choice([2, 3, 4, 5, 6, 8, 10, 12, 15, 20])
        return [m, v], (f"A {m} kg object moves at {v} m/s. "
                        f"Find its momentum.")

    @staticmethod
    def solve(m, v):
        return f"{dec(m * v)} kg m/s"

@relation("newton2", "physics", ["m", "a"], exact=False)
class Newton2:
    @staticmethod
    def gen(rng):
        m = rng.choice([1, 2, 3, 4, 5, 8, 10, 12, 15, 20])
        a = rng.choice([1, 2, 3, 4, 5, 6, 8, 10])
        return [m, a], (f"A net force acts on a {m} kg mass, giving it an "
                        f"acceleration of {a} m/s^2. Find the force.")

    @staticmethod
    def solve(m, a):
        return f"{dec(m * a)} N"

@relation("circular_accel", "physics", ["v", "r"], exact=False)
class CircularAccel:
    @staticmethod
    def gen(rng):
        v = rng.choice([2, 3, 4, 5, 6, 8, 10, 12, 15, 20])
        r = rng.choice([1, 2, 3, 4, 5, 6, 8, 10, 12, 15, 20])
        return [v, r], (f"An object moves at {v} m/s in a circle of radius "
                        f"{r} m. Find its centripetal acceleration.")

    @staticmethod
    def solve(v, r):
        return f"{dec(sp.Rational(v * v, r))} m/s^2"

@relation("wave_speed", "physics", ["f", "lam"], exact=False)
class WaveSpeed:
    @staticmethod
    def gen(rng):
        f = rng.choice([2, 4, 5, 8, 10, 20, 25, 50, 100, 200])
        lam = rng.choice([1, 2, 3, 4, 5, 6, 8, 10, 12, 15])
        return [f, lam], (f"A wave has frequency {f} Hz and wavelength "
                          f"{lam} m. Find its speed.")

    @staticmethod
    def solve(f, lam):
        return f"{dec(f * lam)} m/s"

@relation("specific_heat", "physics", ["m", "c", "dT"], exact=False)
class SpecificHeat:
    @staticmethod
    def gen(rng):
        m = rng.choice([1, 2, 3, 4, 5, 8, 10, 20, 50])
        c = rng.choice([0.9, 1, 2.1, 4.18, 4.2])
        dT = rng.choice([5, 10, 15, 20, 25, 30, 40, 50])
        return [m, c, dT], (f"Find the heat needed to raise {m} kg of a "
                            f"substance with specific heat {c} J/(g C) by "
                            f"{dT} degrees.")

    @staticmethod
    def solve(m, c, dT):
        return f"{dec_exact(sp.Rational(m) * _exact(c) * sp.Rational(dT))} J"

@relation("ohm_v", "physics", ["I", "R"], exact=False)
class OhmV:
    @staticmethod
    def gen(rng):
        I = rng.choice([1, 2, 3, 4, 5, 6, 8, 10, 12])
        R = rng.choice([2, 3, 4, 5, 6, 8, 10, 12, 15, 20])
        return [I, R], (f"A current of {I} A flows through a {R} ohm "
                        f"resistor. Find the voltage across it.")

    @staticmethod
    def solve(I, R):
        return f"{dec(I * R)} V"

@relation("ohm_power", "physics", ["I", "V"], exact=False)
class OhmPower:
    @staticmethod
    def gen(rng):
        I = rng.choice([1, 2, 3, 4, 5, 6, 8, 10, 12])
        V = rng.choice([2, 3, 5, 6, 9, 12, 15, 20, 24])
        return [I, V], (f"A device draws {I} A at {V} V. "
                        f"Find the power it dissipates.")

    @staticmethod
    def solve(I, V):
        return f"{dec(I * V)} W"

@relation("free_fall", "physics", ["h", "g"], exact=False)
class FreeFall:
    @staticmethod
    def gen(rng):
        h = rng.choice([1, 2, 3, 4, 5, 6, 8, 10, 12, 15, 20, 25, 30, 45])
        g = rng.choice([9.8, 9.81, 10])
        return [h, g], (f"An object is dropped from {h} m. Find its speed "
                        f"on impact using g = {g}.")

    @staticmethod
    def solve(h, g):
        return f"{dec(sp.sqrt(2 * sp.Float(h) * sp.Float(g)))} m/s"
