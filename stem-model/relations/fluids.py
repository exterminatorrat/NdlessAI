from .core import relation, _exact, dec_exact

# ---------------------------------------------------------------------------
# Wave 4.4: fluid mechanics.
#
# Every answer is an exact rational of the decimal arguments, Bernoulli's
# equation included: its g is fixed at 9.8 and stated in the question, so the
# static term is a ratio of decimals too. Rounding the exact rational is what
# keeps this catalogue and the C renderer byte-identical.
# ---------------------------------------------------------------------------


@relation("gauge_pressure", "physics", ["rho", "h", "g"], exact=False,
          step=dict(line="P = rho*g*h", expr="@1*@2*@3"))
class GaugePressure:
    @staticmethod
    def gen(rng):
        rho = rng.choice([500, 750, 1000, 1200, 1500, 1800, 2000])
        h = rng.choice([1, 2, 3, 4, 5, 6, 8, 10, 12, 15, 20, 25, 30, 40, 50])
        g = rng.choice([9.8, 9.81, 10])
        return [rho, h, g], (
            f"A tank holds a liquid of density {rho} kg/m^3 to a depth of "
            f"{h} m. Find the gauge pressure at the bottom, using "
            f"g = {g} m/s^2.")

    @staticmethod
    def solve(rho, h, g):
        return f"{dec_exact(_exact(rho) * _exact(h) * _exact(g))} Pa"


@relation("buoyant_force", "physics", ["rho", "V", "g"], exact=False,
          step=dict(line="F = rho*V*g", expr="@1*@2*@3"))
class BuoyantForce:
    @staticmethod
    def gen(rng):
        rho = rng.choice([500, 750, 1000, 1200, 1500, 1800, 2000])
        V = rng.choice([0.001, 0.002, 0.005, 0.01, 0.02])
        g = rng.choice([9.8, 9.81, 10])
        return [rho, V, g], (
            f"A block of volume {V} m^3 is fully submerged in a liquid of "
            f"density {rho} kg/m^3. Find the buoyant force on it, using "
            f"g = {g} m/s^2.")

    @staticmethod
    def solve(rho, V, g):
        return f"{dec_exact(_exact(rho) * _exact(V) * _exact(g))} N"


@relation("continuity_velocity", "physics", ["A1", "v1", "A2"], exact=False,
          step=dict(line="v2 = A1*v1/A2", expr="@1*@2/@3"))
class ContinuityVelocity:
    @staticmethod
    def gen(rng):
        A1, A2 = rng.choice([(0.01, 0.005), (0.02, 0.005), (0.05, 0.01),
                             (0.02, 0.01), (0.005, 0.001), (0.05, 0.02),
                             (0.01, 0.002), (0.02, 0.002)])
        v1 = rng.choice([1, 2, 3, 4, 5, 6, 7, 8, 9, 10])
        return [A1, v1, A2], (
            f"A pipe narrows from a cross-sectional area of {A1} m^2 to "
            f"{A2} m^2. The liquid moves at {v1} m/s in the wide section. "
            f"Find its speed in the narrow section.")

    @staticmethod
    def solve(A1, v1, A2):
        return f"{dec_exact(_exact(A1) * _exact(v1) / _exact(A2))} m/s"


@relation("bernoulli_pressure", "physics",
          ["P1", "v1", "v2", "h1", "h2", "rho"], exact=False,
          step=dict(line="P2 = P1 + rho*(v1^2 - v2^2)/2 + rho*g*(h1 - h2)",
                    expr="@1 + @6*(@2^2 - @3^2)/2 + @6*9.8*(@4 - @5)"))
class BernoulliPressure:
    @staticmethod
    def gen(rng):
        while True:
            P1 = rng.choice([100000, 120000, 150000, 180000, 200000])
            v1 = rng.choice([1, 2, 3, 4, 5, 6, 7, 8, 9, 10])
            v2 = rng.choice([1, 2, 3, 4, 5, 6, 7, 8, 9, 10])
            h1 = rng.choice([0, 1, 2, 3, 4, 5])
            h2 = rng.choice([0, 1, 2, 3, 4, 5])
            rho = rng.choice([500, 800, 1000, 1200, 1500, 2000])
            # A negative absolute pressure is not physical, so instances whose
            # dynamic and static terms would drive P2 below zero are redrawn
            # rather than answered.
            p2 = (P1 + rho * (v1 * v1 - v2 * v2) / 2 + rho * 9.8 * (h1 - h2))
            if p2 > 0:
                break
        return [P1, v1, v2, h1, h2, rho], (
            f"A liquid of density {rho} kg/m^3 flows through a pipe. At "
            f"point 1 the pressure is {P1} Pa, the speed is {v1} m/s and the "
            f"height is {h1} m; at point 2 the speed is {v2} m/s and the "
            f"height is {h2} m. Find the pressure at point 2, taking "
            f"g = 9.8 m/s^2.")

    @staticmethod
    def solve(P1, v1, v2, h1, h2, rho):
        drop = _exact(rho) * (_exact(v1) ** 2 - _exact(v2) ** 2) / 2
        static = _exact(rho) * _exact("9.8") * (_exact(h1) - _exact(h2))
        return f"{dec_exact(_exact(P1) + drop + static)} Pa"


@relation("volume_flow_rate", "physics", ["A", "v"], exact=False,
          step=dict(line="Q = A*v", expr="@1*@2"))
class VolumeFlowRate:
    @staticmethod
    def gen(rng):
        A = rng.choice([0.0005, 0.001, 0.002, 0.005, 0.01, 0.02, 0.05])
        v = rng.choice([1, 2, 3, 4, 5, 6, 7, 8, 9, 10])
        return [A, v], (
            f"A liquid flows at {v} m/s through a pipe of cross-sectional "
            f"area {A} m^2. Find the volume flow rate.")

    @staticmethod
    def solve(A, v):
        return f"{dec_exact(_exact(A) * _exact(v))} m^3/s"


@relation("pascal_press", "physics", ["F1", "A1", "A2"], exact=False,
          step=dict(line="F2 = F1*A2/A1", expr="@1*@3/@2"))
class PascalPress:
    @staticmethod
    def gen(rng):
        A1 = rng.choice([0.001, 0.002, 0.005])
        A2 = rng.choice([0.01, 0.02, 0.05])
        F1 = rng.choice([50, 80, 100, 120, 150, 200, 250, 300, 400, 500])
        return [F1, A1, A2], (
            f"A hydraulic press has an input piston of area {A1} m^2 and an "
            f"output piston of area {A2} m^2. A force of {F1} N is applied "
            f"to the input piston. Find the force on the output piston.")

    @staticmethod
    def solve(F1, A1, A2):
        return f"{dec_exact(_exact(F1) * _exact(A2) / _exact(A1))} N"


@relation("submerged_fraction", "physics", ["rho_obj", "rho_fluid"],
          exact=False, step=dict(line="x = rho_obj/rho_fluid",
                                 expr="@1/@2"))
class SubmergedFraction:
    @staticmethod
    def gen(rng):
        rho_obj = rng.choice([300, 400, 500, 600, 700, 800, 900])
        rho_fluid = rng.choice([1000, 1030, 1200, 1500, 2000])
        return [rho_obj, rho_fluid], (
            f"An object of density {rho_obj} kg/m^3 floats in a liquid of "
            f"density {rho_fluid} kg/m^3. Find the fraction of its volume "
            f"that lies below the surface.")

    @staticmethod
    def solve(rho_obj, rho_fluid):
        return dec_exact(_exact(rho_obj) / _exact(rho_fluid))
