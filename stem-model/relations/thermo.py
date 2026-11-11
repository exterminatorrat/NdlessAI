import sympy as sp

from .core import relation, _exact, dec_exact

# ---------------------------------------------------------------------------
# Wave 3.4: the rest of thermodynamics.
#
# Everything here renders an exact rational of the decimal arguments. These
# are the questions a student checks by hand, so 918.5 W has to print as
# 918.5 W, and rounding the exact rational is what keeps this catalogue and
# the C renderer byte-identical.
# ---------------------------------------------------------------------------


@relation("gas_work_isobaric", "physics", ["P", "dV"], exact=False,
          step=dict(line="W = P*dV", expr="@1 * @2"))
class GasWorkIsobaric:
    @staticmethod
    def gen(rng):
        P = rng.choice([100, 200, 500, 1000, 2000])
        dV = rng.choice([0.1, 0.2, 0.5, 1, 2])
        return [P, dV], (f"A gas expands at a constant pressure of {P} Pa "
                         f"through a volume change of {dV} m^3. Find the work "
                         f"done by the gas.")

    @staticmethod
    def solve(P, dV):
        return f"{dec_exact(_exact(P) * _exact(dV))} J"


@relation("gas_heat_cp", "physics", ["n", "cp", "dT"], exact=False,
          step=dict(line="Q = n*cp*dT", expr="@1*@2*@3"))
class GasHeatCp:
    @staticmethod
    def gen(rng):
        n = rng.choice([0.5, 1, 2, 3])
        cp = rng.choice([20.8, 29.1, 37.1, 75.3])
        dT = rng.choice([10, 20, 25, 50])
        return [n, cp, dT], (f"{n} mol of a gas with molar heat capacity "
                             f"{cp} J/(mol K) is heated by {dT} K at constant "
                             f"pressure. Find the heat absorbed.")

    @staticmethod
    def solve(n, cp, dT):
        return f"{dec_exact(_exact(n) * _exact(cp) * _exact(dT))} J"


@relation("entropy_change", "physics", ["q", "T"], exact=False,
          step=dict(line="dS = Q/T", expr="@1 / @2"))
class EntropyChange:
    @staticmethod
    def gen(rng):
        q = rng.choice([100, 200, 500, 1000, 2500])
        T = rng.choice([200, 250, 300, 350, 400])
        return [q, T], (f"{q} J of heat is transferred reversibly at a "
                        f"constant {T} K. Find the entropy change.")

    @staticmethod
    def solve(q, T):
        return f"{dec_exact(_exact(q) / _exact(T))} J/K"


@relation("conduction_rate", "physics", ["k", "A", "dT", "d"], exact=False,
          step=dict(line="P = k*A*dT/d", expr="@1*@2*@3/@4"))
class ConductionRate:
    @staticmethod
    def gen(rng):
        k = rng.choice([0.1, 0.2, 0.5, 1, 400])
        A = rng.choice([0.5, 1, 2, 5])
        dT = rng.choice([10, 20, 50, 100])
        d = rng.choice([0.01, 0.02, 0.05, 0.1, 1])
        return [k, A, dT, d], (
            f"A slab of thermal conductivity {k} W/(m K) has area {A} m^2 and "
            f"thickness {d} m. The faces differ by {dT} K. Find the rate of "
            f"heat conduction.")

    @staticmethod
    def solve(k, A, dT, d):
        return f"{dec_exact(_exact(k) * _exact(A) * _exact(dT) / _exact(d))} W"


@relation("radiation_power", "physics", ["T", "area"], exact=False,
          step=dict(line="P = sigma*A*T^4", expr="5.67e-8*@2*@1^4"))
class RadiationPower:
    @staticmethod
    def gen(rng):
        T = rng.choice([200, 250, 300, 350, 400])
        area = rng.choice([0.5, 1, 2, 3])
        return [T, area], (
            f"A black body at {T} K has surface area {area} m^2. Find the "
            f"power it radiates. Use sigma = 5.67e-8 W/(m^2 K^4).")

    @staticmethod
    def solve(T, area):
        sigma = _exact("5.67e-8")
        return f"{dec_exact(sigma * _exact(area) * _exact(T) ** 4)} W"


@relation("heat_pump_cop", "physics", ["t_cold", "t_hot"], exact=False,
          step=dict(line="COP = Tc/(Th - Tc)", expr="@1 / (@2 - @1)"))
class HeatPumpCop:
    @staticmethod
    def gen(rng):
        t_cold = rng.choice([250, 270, 280, 290])
        t_hot = t_cold + rng.choice([20, 40, 50, 70])
        return [t_cold, t_hot], (
            f"A heat pump moves heat from {t_cold} K to {t_hot} K. Find its "
            f"coefficient of performance.")

    @staticmethod
    def solve(t_cold, t_hot):
        return dec_exact(_exact(t_cold) / (_exact(t_hot) - _exact(t_cold)))
