import random

TEMPLATES = {
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
