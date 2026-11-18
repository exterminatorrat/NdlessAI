"""The unit each relation's solver expects, per argument, in declaration order.

The runtime reads the unit a question writes after each number and compares it
with the slot's unit here (runtime/quantity.c): the same unit passes, a unit of
the same kind is converted exactly, and a unit of another kind is refused,
because a mass in a speed slot is an argument in the wrong place.

Every unit is the one the relation's own generator writes after that argument,
so the table states what the solver already assumes rather than choosing
anything new. "-" declares nothing and the runtime leaves that slot alone. It
is used for pure numbers, coefficients, percentages, money, and on purpose for
every temperature difference: "a 20 degree rise" is neither an angle nor a
Celsius reading, and converting it with the 273.15 offset would be wrong.

A slot declares a unit only when the solver fixes it: the answer carries a
unit label ("J", "atm", "km/h"), or the formula needs an absolute unit (kelvin
in a gas law, degrees inside a sine, seconds behind becquerels). A slot whose
solver only needs consistent units, and answers in whatever unit it was given,
declares nothing, because converting it changes a right answer. Half-life is
the clear case: 80 g halving every 2 days for 4 days is 20 in days, hours or
years alike, and rewriting the days as years only hands the solver fractions
it refuses. Interest and growth durations declare nothing for a second reason:
the rate's own time basis ("5% per day") is not a slot, so converting the
duration alone to years would silently change the answer.

A slot also declares nothing where the corpus shows the unit is free or the
wording cannot be read safely. Area, volume, combined work rates and half-life
ratios answer in whatever unit they are given, so converting them would change
a right answer. "Reaches 25 m/s in one second" is a correct acceleration
written as a speed, and "0.007 moles of ions per litre" puts a concentration's
unit on either side of other words, and
"one mole weighs 44 g" and "0.005 kg of mass in every metre" write a
molar mass and a linear density as plain masses. Declaring those would refuse questions the
model already answers correctly; a swap there is still caught through the
relation's other slots wherever they declare a unit.

A slot may list alternatives, "M|mol", when its solver only takes the ratio
of two arguments that can be stated either way: a buffer's base and acid in
moles in one solution give the same pH as their molarities, and a percent yield
is the same in moles as in grams. Each number converts within the kind it was
written in, and the two sides must be written in the same kind, because 2 mol
over 3 M is not a ratio.

A relation missing from this table declares nothing for any slot. Names must be
spelled the way quantity.c spells a unit's canonical form; test_quantity fails
on a name it does not know, and export_relations.py on a wrong argument count.
"""

UNITS = {
    "activity_bq": ("-", "s"),
    "adiabatic_work": ("Pa", "m^3", "Pa", "m^3", "-"),
    "arc_length": ("-", "degrees"),
    "average_speed": ("km", "hours"),
    "avg_rate": ("M", "s"),
    "beat_freq": ("Hz", "Hz"),
    "bernoulli_pressure": ("Pa", "m/s", "m/s", "m", "m", "kg/m^3"),
    "boyles": ("atm", "L", "L"),
    "buffer_ph": ("-", "M|mol", "M|mol"),
    "buoyant_force": ("kg/m^3", "m^3", "m/s^2"),
    "calorimetry_c": ("J", "g", "-"),
    "calorimetry_mass": ("J", "-", "-"),
    "capacitor_charge": ("uF", "V"),
    "capacitor_energy": ("uC", "V"),
    "carnot": ("K", "K"),
    "carnot_efficiency": ("K", "K"),
    "celsius_to_fahrenheit": ("degC",),
    "charles": ("L", "K", "K"),
    "circular_accel": ("m/s", "m"),
    "collision_velocity": ("kg", "m/s", "kg", "m/s"),
    "combined_gas": ("atm", "L", "K", "atm", "K"),
    "conduction_rate": ("-", "m^2", "-", "m"),
    "continuity_velocity": ("m^2", "m/s", "m^2"),
    "cos_eval": ("degrees",),
    "coulomb_force": ("uC", "uC", "m"),
    "de_broglie_electron": ("V",),
    "density": ("g", "mL"),
    "dilution": ("M", "mL", "mL"),
    "dilution_v1": ("M", "M", "mL"),
    "doppler_freq": ("Hz", "m/s", "m/s"),
    "double_slit_fringe": ("-", "nm", "m", "mm"),
    "electric_field_point": ("uC", "m"),
    "energy_friction": ("kg", "m/s", "N", "m"),
    "entropy_change": ("J", "K"),
    "fahrenheit_to_celsius": ("degF",),
    "first_law": ("J", "J"),
    "free_fall": ("m", "m/s^2"),
    "friction_force": ("-", "N"),
    "gas_density": ("Pa", "kg/mol", "K"),
    "gas_heat_cp": ("mol", "-", "-"),
    "gas_p": ("mol", "K", "L"),
    "gas_work_isobaric": ("Pa", "m^3"),
    "gauge_pressure": ("kg/m^3", "m", "m/s^2"),
    "gibbs": ("kJ", "K", "J/K"),
    "heat_pump_cop": ("K", "K"),
    "heat_reaction": ("mol", "kJ/mol"),
    "impulse": ("N", "s"),
    "internal_energy_gas": ("K",),
    "kc_from_conc": ("M", "M", "-"),
    "kg_to_lb": ("kg",),
    "kinematic_v": ("m/s", "-", "s"),
    "kinetic_energy": ("kg", "m/s"),
    "km_to_m": ("km",),
    "latent_heat": ("g", "J/g"),
    "law_cosines": ("-", "-", "degrees"),
    "law_sines": ("-", "degrees", "degrees"),
    "lens_image_distance": ("cm", "cm"),
    "lens_magnification": ("cm", "cm"),
    "limiting_moles": ("g", "g/mol", "g", "g/mol"),
    "liters_to_ml": ("L",),
    "m_to_cm": ("m",),
    "mass_energy_mev": ("u",),
    "mass_percent": ("g", "g"),
    "mirror_image_distance": ("cm", "cm"),
    "molality": ("mol", "kg"),
    "molarity": ("mol", "L"),
    "mole_fraction": ("mol", "mol"),
    "moles": ("g", "-"),
    "moles_from_molarity": ("mol/L", "L"),
    "momentum": ("kg", "m/s"),
    "motional_emf": ("T", "m", "m/s"),
    "newton2": ("kg", "-"),
    "ohm_i": ("V", "ohm"),
    "ohm_power": ("A", "V"),
    "ohm_v": ("A", "ohm"),
    "parallel_resistance": ("ohm", "ohm"),
    "pascal_press": ("N", "m^2", "m^2"),
    "pendulum_period": ("m", "m/s^2"),
    "percent_yield": ("g|mol", "g|mol"),
    "photoelectric_kev": ("nm", "eV"),
    "photon_energy_ev": ("nm",),
    "potential_energy": ("kg", "m", "m/s^2"),
    "power": ("J", "s"),
    "power_force": ("N", "m/s"),
    "proj_range": ("m/s", "degrees", "m/s^2"),
    "radiation_power": ("K", "m^2"),
    "rms_speed": ("K", "kg/mol"),
    "series_resistance": ("ohm", "ohm", "ohm"),
    "sin_eval": ("degrees",),
    "snell_angle": ("-", "degrees", "-"),
    "sound_intensity": ("dB",),
    "specific_heat": ("-", "-", "-"),
    "spring_energy": ("N/m", "m"),
    "standing_wave_freq": ("-", "m/s", "m"),
    "stoich_mass": ("g", "g/mol", "g/mol", "-"),
    "stoichiometry": ("mol", "-"),
    "string_freq": ("N", "-", "m"),
    "submerged_fraction": ("kg/m^3", "kg/m^3"),
    "tan_eval": ("degrees",),
    "threshold_wavelength": ("eV",),
    "torque": ("N", "m", "degrees"),
    "volume_flow_rate": ("m^2", "m/s"),
    "wave_speed": ("Hz", "m"),
    "work": ("N", "m", "degrees"),
    "work_energy": ("kg", "m/s", "m/s"),
}
