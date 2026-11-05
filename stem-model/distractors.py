import random

TEMPLATES = {
    "sin_eval": "A second problem in the same set asks for sin({n}).",
    "cos_eval": "A second problem in the same set asks for cos({n}).",
    "tan_eval": "A second problem in the same set asks for tan({n}).",
    "pythagoras": "A second triangle on the same page has a leg of {n}.",
    "law_sines": "A second triangle in the same exercise has a side of {n}.",
    "law_cosines": "A second triangle in the same exercise has a side of {n}.",
    "arc_length": "A second circle in the same exercise has radius {n}.",
    "log_eval": "A second problem in the same set asks for log base {n}.",
    "ln_eval": "A second problem in the same set asks for ln({n}).",
    "log10_eval": "A second problem in the same set asks for log({n}).",
    "compound_interest": "A second account in the same problem earns {n}%.",
    "continuous_growth": "A second population in the same study grows at {n}%.",
    "half_life": "A second sample in the same lab has a half life of {n} years.",
    "doubling_time": "A second fund in the same table grows at {n}%.",
    "arith_seq": "A second sequence in the same exercise has first term {n}.",
    "geom_seq": "A second sequence in the same exercise has first term {n}.",
    "abs_value_eq": "A second equation in the same set has right side {n}.",
    "linear_ineq": "A second inequality in the same set has right side {n}.",
    "system2": "A second system in the same section has constant {n}.",
    "complete_square": "A second parabola in the same exercise has constant {n}.",
    "factor_quadratic": "A second expression in the same set has constant {n}.",
    "kinetic_energy": "A second object in the same problem has mass {n} kg.",
    "potential_energy": "A second mass in the same problem is {n} kg.",
    "work": "A second force in the same problem is {n} N.",
    "power": "A second device in the same problem uses {n} J.",
    "momentum": "A second object in the same problem has mass {n} kg.",
    "newton2": "A second mass in the same problem is {n} kg.",
    "circular_accel": "A second object in the same problem has radius {n} m.",
    "wave_speed": "A second wave in the same problem has frequency {n} Hz.",
    "specific_heat": "A second sample in the same problem has mass {n} kg.",
    "ohm_v": "A second resistor in the same circuit is {n} ohm.",
    "ohm_power": "A second device in the same circuit draws {n} A.",
    "free_fall": "A second object in the same problem is dropped from {n} m.",
    "molarity": "A second solution in the same problem contains {n} mol.",
    "ph": "A second solution in the same problem has concentration {n} mol/L.",
    "poh": "A second solution in the same problem has concentration {n} mol/L.",
    "ph_from_poh": "A second solution in the same problem has pOH {n}.",
    "percent_yield": "A second trial in the same lab produced {n} g.",
    "boyles": "A second cylinder in the same problem holds {n} L.",
    "charles": "A second sample in the same problem is at {n} K.",
    "combined_gas": "A second sample in the same problem is at {n} K.",
    "density": "A second sample in the same problem has mass {n} g.",
    "stoichiometry": "A second reaction in the same problem uses {n} mol.",
    "km_to_m": "A second distance in the same problem is {n} km.",
    "m_to_cm": "A second length in the same problem is {n} m.",
    "celsius_to_fahrenheit": "A second reading in the same problem is {n} degrees Celsius.",
    "fahrenheit_to_celsius": "A second reading in the same problem is {n} degrees Fahrenheit.",
    "kg_to_lb": "A second mass in the same problem is {n} kg.",
    "liters_to_ml": "A second volume in the same problem is {n} L.",
    "percent_error": "A second trial in the same lab measured {n}.",
    "scientific_notation": "A second value in the same set is {n} x 10^3.",
    "average": "A fifth value in the same set is {n}.",
    "quad_solve": "A second equation on the same worksheet has constant term {n}.",
    "deriv_poly3": "A separate part of the question gives x = {n} for a related curve.",
    "integr_poly3": "A second integral in the same set runs over an interval of width {n}.",
    "linear2": "An earlier system in the same section had constant term {n}.",
    "exp_solve": "A parallel exercise uses a base of {n}.",
    "kinematic_v": "A second trial in the same session recorded a speed of {n} m/s.",
    "proj_range": "A previous launch in the same report used {n} m/s.",
    "ohm_i": "The bench also holds a resistor rated {n} ohm.",
    "moles": "The same shelf holds a second sample, {n} g of a different compound.",
    "dilution": "The cupboard also holds a volumetric flask of {n} mL.",
    "gas_p": "A second cylinder in the same rack is rated {n} L.",
}

POOLS = {
    "sin_eval": [10, 20, 30, 40, 50, 60, 70, 80],
    "exp_solve": [2, 3, 4, 5, 6, 7, 8, 10],
    "dilution": [10, 25, 50, 100, 150, 200, 250, 500],
    "gas_p": [1, 2, 5, 10, 20, 22.4],
    "ohm_i": [2, 3, 4, 5, 6, 8, 10, 12, 15, 20],
    "proj_range": [5, 10, 15, 20, 25, 30, 35, 40],
    "kinematic_v": [2, 5, 8, 10, 12, 15, 18, 20],
    "moles": [5, 10, 11, 20, 22, 44, 50, 88],
}


def pick_number(name, params, rng):
    taken = {str(p) for p in params}
    pool = [n for n in POOLS.get(name, []) if str(n) not in taken]
    if pool:
        return rng.choice(pool)
    while True:
        n = rng.choice([rng.randint(2, 99), rng.randint(2, 20) * 5])
        if str(n) not in taken:
            return n


def add_distractor(name, params, question, rng):
    n = pick_number(name, params, rng)
    return f"{question} {TEMPLATES[name].format(n=n)}", n
