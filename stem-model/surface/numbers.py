"""Number and unit rendering, and the formats a slot must not use.

The grounding rule matches digit sequences on a full token, so a value written
as `6.3e-05` is not the same text as `0.000063` and the call and the question
disagree. That defect reached a gate once already. Here it is decided per slot
rather than per sentence: a value this module cannot write plainly returns
`None`, and the generator drops the row instead of hoping.
"""

# A value must be writable without an exponent to survive grounding.
SMALLEST = 1e-4
LARGEST = 1e7


def canon(value):
    """The text of a value as a student would write it, or None if it needs an exponent."""
    if isinstance(value, int):
        return str(value)
    number = float(value)
    if number == 0:
        return "0"
    if abs(number) < SMALLEST or abs(number) >= LARGEST:
        return None
    text = f"{number:.6f}".rstrip("0").rstrip(".")
    return text or "0"


def with_unit(value_text, unit):
    """`0.2 mol`, and no trailing space for a dimensionless slot."""
    return f"{value_text} {unit}".strip()


def spell(register, value_text, unit):
    """A value with its unit; the register does not change how a unit is written."""
    return with_unit(value_text, unit)
