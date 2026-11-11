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

@relation("kinetic_energy", "physics", ["m", "v"], exact=False,
          step=dict(line="KE = m*v^2 / 2", expr="@1 * @2^2 / 2"))
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

@relation("potential_energy", "physics", ["m", "h", "g"], exact=False,
          step=dict(line="PE = m*g*h", expr="@1 * @2 * @3"))
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

@relation("power", "physics", ["W", "t"], exact=False,
          step=dict(line="P = W/t", expr="@1 / @2"))
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

@relation("momentum", "physics", ["m", "v"], exact=False,
          step=dict(line="p = m*v", expr="@1 * @2"))
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

@relation("newton2", "physics", ["m", "a"], exact=False,
          step=dict(line="F = m*a", expr="@1 * @2"))
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

@relation("specific_heat", "physics", ["m", "c", "dT"], exact=False,
          step=dict(line="Q = m*c*dT", expr="@1 * @2 * @3"))
class SpecificHeat:
    @staticmethod
    def gen(rng):
        m = rng.choice([50, 100, 150, 200, 250, 300, 400, 500, 750, 1000])
        c = rng.choice([0.9, 1, 2.1, 4.18, 4.2])
        dT = rng.choice([5, 10, 15, 20, 25, 30, 40, 50])
        return [m, c, dT], (f"Find the heat needed to raise {m} g of a "
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

@relation("friction_force", "physics", ["mu", "N"], exact=False)
class FrictionForce:
    @staticmethod
    def gen(rng):
        mu = rng.choice([0.1, 0.15, 0.2, 0.25, 0.3, 0.35, 0.4, 0.45, 0.5, 0.6])
        N = rng.choice([50, 80, 100, 120, 150, 200, 250, 300, 400, 500])
        return [mu, N], (f"A crate on a rough floor feels a normal force of "
                         f"{N} N. The coefficient of friction is {mu}. Find "
                         f"the friction force.")

    @staticmethod
    def solve(mu, N):
        return f"{dec_exact(_exact(mu) * sp.Rational(N))} N"

@relation("work_energy", "physics", ["m", "v0", "v"], exact=False)
class WorkEnergy:
    @staticmethod
    def gen(rng):
        m = rng.choice([1, 2, 3, 4, 5, 8, 10, 12, 15, 20])
        v0 = rng.choice([0, 2, 3, 4, 5, 6, 8, 10, 12])
        v = v0 + rng.choice([2, 3, 4, 5, 6, 8, 10, 12])
        return [m, v0, v], (f"A {m} kg cart speeds up from {v0} m/s to {v} "
                            f"m/s. Find the net work done on it.")

    @staticmethod
    def solve(m, v0, v):
        gain = sp.Rational(v * v - v0 * v0, 2)
        return f"{dec_exact(sp.Rational(m) * gain)} J"

@relation("power_force", "physics", ["F", "v"], exact=False)
class PowerForce:
    @staticmethod
    def gen(rng):
        F = rng.choice([10, 20, 25, 30, 40, 50, 60, 80, 100, 120])
        v = rng.choice([2, 3, 4, 5, 6, 8, 10, 12, 15, 20])
        return [F, v], (f"A motor pulls a load with a constant force of {F} N "
                        f"while moving it at {v} m/s. Find the power output.")

    @staticmethod
    def solve(F, v):
        return f"{dec_exact(sp.Rational(F) * sp.Rational(v))} W"

@relation("energy_friction", "physics", ["m", "v0", "f", "d"], exact=False)
class EnergyFriction:
    @staticmethod
    def gen(rng):
        while True:
            m = rng.choice([1, 2, 3, 4, 5, 8, 10, 12, 15, 20])
            v0 = rng.choice([6, 8, 10, 12, 15, 20])
            d = rng.choice([2, 3, 4, 5, 6, 8, 10, 12])
            pool = [f for f in (5, 10, 15, 20, 25, 30, 40, 50, 60, 80)
                    if 2 * f * d < m * v0 * v0]
            if pool:
                break
        f = rng.choice(pool)
        return [m, v0, f, d], (f"A {m} kg block slides at {v0} m/s on a rough "
                               f"floor. A constant friction force of {f} N "
                               f"acts over {d} m. Find its final speed.")

    @staticmethod
    def solve(m, v0, f, d):
        # Friction cannot remove more kinetic energy than the block has: when
        # the work it would do exceeds mv0^2/2 the block stops before the end
        # of the distance and the final speed is zero, not undefined.
        left = sp.Rational(m * v0 * v0 - 2 * f * d, m)
        if left <= 0:
            return "0 m/s"
        return f"{dec(sp.sqrt(sp.Float(left)))} m/s"

@relation("impulse", "physics", ["F", "t"], exact=False,
          step=dict(line="J = F*t", expr="@1 * @2"))
class Impulse:
    @staticmethod
    def gen(rng):
        F = rng.choice([5, 10, 15, 20, 25, 30, 40, 50, 60, 80, 100])
        t = rng.choice([1, 2, 3, 4, 5, 6, 8, 10])
        return [F, t], (f"A constant force of {F} N acts on a ball for {t} s. "
                        f"Find the impulse delivered.")

    @staticmethod
    def solve(F, t):
        return f"{dec_exact(sp.Rational(F) * sp.Rational(t))} N s"

@relation("spring_energy", "physics", ["k", "x"], exact=False,
          step=dict(line="E = k*x^2 / 2", expr="@1 * @2^2 / 2"))
class SpringEnergy:
    @staticmethod
    def gen(rng):
        k = rng.choice([50, 80, 100, 120, 150, 200, 250, 300, 400, 500])
        x = rng.choice([0.05, 0.1, 0.15, 0.2, 0.25, 0.3, 0.4, 0.5])
        return [k, x], (f"A spring with constant {k} N/m is compressed {x} m. "
                        f"Find the elastic potential energy stored.")

    @staticmethod
    def solve(k, x):
        return f"{dec_exact(sp.Rational(k) * _exact(x) ** 2 / 2)} J"

@relation("pendulum_period", "physics", ["L", "g"], exact=False)
class PendulumPeriod:
    @staticmethod
    def gen(rng):
        L = rng.choice([0.25, 0.5, 0.75, 1, 1.5, 2, 2.5, 3, 4])
        g = rng.choice([9.8, 9.81, 10])
        return [L, g], (f"A pendulum is {L} m long. Find its period using "
                        f"g = {g} m/s^2.")

    @staticmethod
    def solve(L, g):
        return f"{dec(2 * sp.pi * sp.sqrt(sp.Float(L) / sp.Float(g)))} s"

@relation("torque", "physics", ["F", "r", "theta"], exact=False)
class Torque:
    @staticmethod
    def gen(rng):
        F = rng.choice([10, 20, 25, 30, 40, 50, 60, 80, 100])
        r = rng.choice([1, 2, 3, 4, 5, 6, 8, 10, 12])
        theta = rng.choice([30, 45, 60, 90, 120, 150])
        return [F, r, theta], (f"A force of {F} N acts {r} m from a pivot at "
                               f"{theta} degrees to the lever arm. Find the "
                               f"torque about the pivot.")

    @staticmethod
    def solve(F, r, theta):
        return f"{dec(sp.Float(F) * sp.Float(r) * sp.sin(sp.rad(theta)))} N m"
