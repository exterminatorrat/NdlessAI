"""Hand-written paraphrases of every relation.

None of this wording appears in the training data: the generator emits one
fixed sentence per relation, so `data/val.jsonl` only ever tests new numbers
against familiar phrasing. These cases test whether the model generalises to
how a person would actually type the question.
"""

CASES = [
    ("quad_solve", (1, -3, 2), "Find the roots of x^2 - 3x + 2."),
    ("quad_solve", (1, 0, -9), "For what values of x does x^2 - 9 equal zero?"),
    ("quad_solve", (2, -6, 4), "Factor and solve 2x^2 - 6x + 4 = 0."),

    ("deriv_poly3", (1, 0, 0, 0, 3), "What is the slope of f(x) = x^3 when x = 3?"),
    ("deriv_poly3", (2, -1, 0, 5, -1), "Differentiate 2x^3 - x^2 + 5 and evaluate at x = -1."),
    ("deriv_poly3", (0, 1, 0, 0, 4), "Find f'(4) for f(x) = x^2."),

    ("integr_poly3", (1, 0, 0, 0, 0, 2), "What is the area under x^3 between x = 0 and x = 2?"),
    ("integr_poly3", (3, 2, 2, -1, 1, 4), "Integrate 3x^3 + 2x^2 + 2x - 1 from 1 to 4."),

    ("linear2", (1, 1, 7, 1, -1, 1), "Find x and y given x + y = 7 and x - y = 1."),
    ("linear2", (3, 1, 1, 1, 2, -8), "Two equations: 3x + y = 1, x + 2y = -8. Solve them."),

    ("exp_solve", (3, 81), "If 3^x = 81, what is x?"),
    ("exp_solve", (10, 10000), "Find the exponent: 10 to what power gives 10000?"),

    ("kinematic_v", (5, 2, 4), "A car speeds up from 5 m/s at 2 m/s^2 for 4 s. How fast is it going now?"),
    ("kinematic_v", (20, -4, 3), "A ball thrown up at 20 m/s decelerates at 4 m/s^2 for 3 s. What is its speed?"),

    ("proj_range", (15, 50, 9.8), "How far does a ball fly if kicked at 15 m/s at 50 degrees, with g = 9.8?"),
    ("proj_range", (30, 45, 10), "Find the horizontal distance travelled by a projectile launched at 30 m/s, 45 degrees, g = 10."),

    ("ohm_i", (12, 4), "A 12 V battery is across a 4 ohm resistor. What current flows?"),
    ("ohm_i", (2, 3), "How many amps through 3 ohms with 2 volts applied?"),

    ("moles", (44, 100), "Convert 44 g of a substance with molar mass 100 g/mol into moles."),
    ("moles", (5, 100), "How many mol are in 5 grams when one mole weighs 100 g?"),

    ("dilution", (4, 50, 250), "Dilute 50 mL of 4 M solution up to 250 mL. What is the new concentration?"),
    ("dilution", (1, 50, 250), "You take 50 mL of a 1 M solution and add water to 250 mL. Find the molarity."),

    ("gas_p", (1, 300, 2), "What pressure do 1 mol of gas exert at 300 K in 2 L? Use R = 0.08206."),
    ("gas_p", (0.5, 300, 1), "Calculate the pressure of 0.5 mol of ideal gas occupying 1 L at 300 K, R = 0.08206."),
]
