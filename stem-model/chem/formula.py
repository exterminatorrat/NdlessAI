"""Parse a chemical formula into a composition, a molar mass and ion counts.

The parser is exact and it refuses rather than guessing. Four failures are
deliberate: an element symbol that does not exist, an unbalanced or empty
group, a count that is not a positive integer, and a formula larger than this
build will hold. A wrong molar mass is worse than a refusal because it is
indistinguishable from a right one.

Grammar, which is the one a student writes:

    formula  := segment ('.' segment)*
    segment  := [multiplier] term+
    term     := ('(' formula ')') | symbol [count]
    count    := digits, also written as unicode subscripts

`.` (or `·`) separates the parts of a hydrate: CuSO4.5H2O is copper sulfate
pentahydrate, and the 5 multiplies only the water. A charge written after the
formula, as in `Ba2+`, is read and kept aside: it does not change the mass, but
an ion is not a formula unit, so ion counts refuse it.

`ion_counts` answers "how many cations and how many anions does one formula
unit supply", which is what a Ksp or a conductivity question needs. It refuses
anything that is not clearly ionic: a molecular formula, a hydrate, a charged
species, an anion it does not know, and a formula whose charges do not balance.
"""

import re
from fractions import Fraction

from .elements import (
    ATOMIC_WEIGHTS,
    FIXED_CATION_CHARGES,
    METALS,
    MONATOMIC_ANION_CHARGES,
    NAMES,
    POLYATOMIC_ANIONS,
    POLYATOMIC_CATIONS,
)

_SUBSCRIPTS = str.maketrans("₀₁₂₃₄₅₆₇₈₉", "0123456789")
MAX_NESTING = 8
MAX_ATOMS = 1000
MAX_COUNT = 999

_ELEMENT = re.compile(r"[A-Z][a-z]?")
_COUNT = re.compile(r"[0-9]+")
_CHARGE = re.compile(r"[0-9]*[+-]$")


class FormulaError(ValueError):
    """A formula this module will not answer for, with the reason named."""


def normalise(text):
    """ASCII digits and a dot for every hydrate separator, nothing else."""

    if text is None:
        raise FormulaError("empty formula")
    s = text.strip().translate(_SUBSCRIPTS)
    s = s.replace("·", ".").replace("•", ".").replace("*", ".")
    if not s:
        raise FormulaError("empty formula")
    return s


def _prepare(text):
    """The formula without a trailing charge, and that charge as a number.

    A charge may be written tightly (`Ba2+`) or as its own token
    (`Ba(NO3)2 2+`). Joining the two before reading the count would turn the
    charge's 2 into a subscript of the group, so the space is handled here
    rather than stripped later.
    """

    s = normalise(text)
    charge = 0
    if re.search(r"\s", s):
        parts = s.split()
        if len(parts) != 2 or not _CHARGE.fullmatch(parts[1]):
            raise FormulaError("unexpected space in a formula")
        s, tail = parts
        charge = int(tail[:-1] or 1) * (1 if tail[-1] == "+" else -1)
    m = _CHARGE.search(s)
    if m is not None:
        if charge:
            raise FormulaError("two charges in one formula")
        tail = s[m.start():]
        body = s[:m.start()]
        if not body:
            raise FormulaError("a charge is not a formula")
        charge = int(tail[:-1] or 1) * (1 if tail[-1] == "+" else -1)
        s = body
    if not s:
        raise FormulaError("empty formula")
    return s, charge


def _count(s, i):
    m = _COUNT.match(s, i)
    if m is None:
        return 1, i
    n = int(m.group(0))
    if n < 1 or n > MAX_COUNT:
        raise FormulaError("subscript out of range")
    return n, m.end()


def _sequence(s, i, comp, depth, total):
    """Parse terms until a closing paren or the end; return the next index."""

    seen = False
    while i < len(s) and s[i] != ")":
        if s[i] == "(":
            if depth >= MAX_NESTING:
                raise FormulaError("groups nested too deeply")
            sub = {}
            sub_total = [0]
            i = _sequence(s, i + 1, sub, depth + 1, sub_total)
            if i >= len(s) or s[i] != ")":
                raise FormulaError("unbalanced parenthesis")
            i += 1
            n, i = _count(s, i)
            for symbol, count in sub.items():
                comp[symbol] = comp.get(symbol, 0) + count * n
            total[0] += sub_total[0] * n
        else:
            m = _ELEMENT.match(s, i)
            if m is None:
                raise FormulaError("unexpected character")
            symbol = m.group(0)
            if symbol not in ATOMIC_WEIGHTS:
                raise FormulaError("unknown element " + symbol)
            i = m.end()
            n, i = _count(s, i)
            comp[symbol] = comp.get(symbol, 0) + n
            total[0] += n
        if total[0] > MAX_ATOMS:
            raise FormulaError("formula is larger than this build holds")
        seen = True
    if not seen:
        raise FormulaError("empty group")
    return i


def _segment(s):
    """One hydrate part: an optional multiplier and then terms."""

    n = 1
    m = _COUNT.match(s)
    if m is not None:
        n = int(m.group(0))
        if n < 1 or n > MAX_COUNT:
            raise FormulaError("hydrate multiplier out of range")
        s = s[m.end():]
        if not s:
            raise FormulaError("a multiplier is not a formula")
    comp = {}
    total = [0]
    i = _sequence(s, 0, comp, 0, total)
    if i != len(s):
        raise FormulaError("unbalanced parenthesis")
    if n != 1:
        comp = {k: v * n for k, v in comp.items()}
        total[0] *= n
    return comp, total[0]


def composition(text):
    """Element symbol -> atom count. Raises FormulaError on anything else."""

    s, _charge = _prepare(text)
    if s.count("(") != s.count(")"):
        raise FormulaError("unbalanced parenthesis")
    parts = s.split(".")
    if any(not part for part in parts):
        raise FormulaError("empty hydrate part")
    if parts[0][0].isdigit():
        raise FormulaError("a coefficient is not a formula")
    if len(parts) > MAX_NESTING:
        raise FormulaError("too many hydrate parts")
    comp = {}
    total = 0
    for part in parts:
        sub, n = _segment(part)
        total += n
        for symbol, count in sub.items():
            comp[symbol] = comp.get(symbol, 0) + count
    if total == 0:
        raise FormulaError("empty formula")
    if total > MAX_ATOMS:
        raise FormulaError("formula is larger than this build holds")
    return comp


def molar_mass_mg(text):
    """The molar mass in integer thousandths of a gram per mole."""

    _formula, comp = resolve(text)
    total = 0
    for symbol, count in comp.items():
        total += ATOMIC_WEIGHTS[symbol] * count
    return total


def molar_mass(text):
    """The molar mass as an exact Fraction of grams per mole."""

    return Fraction(molar_mass_mg(text), 1000)


def _split_term(text):
    """A remainder term as (kind, key, count); kinds are group, element, poly."""

    if text.startswith("("):
        depth = 0
        for i, ch in enumerate(text):
            if ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
                if depth == 0:
                    break
        else:
            raise FormulaError("unbalanced parenthesis")
        inner = text[1:i]
        rest = text[i + 1:]
        if "(" in rest or ")" in rest:
            raise FormulaError("unbalanced parenthesis")
        n, used = _count(rest, 0)
        if used != len(rest) or not inner:
            raise FormulaError("unexpected characters after a group")
        return "group", inner, n
    m = _ELEMENT.match(text)
    if m is not None and m.end() == len(text.rstrip("0123456789")):
        symbol = m.group(0)
        rest = text[m.end():]
        n, used = _count(rest, 0)
        if used != len(rest):
            raise FormulaError("unexpected characters after a symbol")
        if symbol not in ATOMIC_WEIGHTS:
            raise FormulaError("unknown element " + symbol)
        return "element", symbol, n
    if _CHARGE.search(text):
        raise FormulaError("a charge is not a formula")
    return "poly", text, 1


def _leading(text):
    """The cation as (key, count, rest) or raise when it is not a metal."""

    if text.startswith("("):
        depth = 0
        for i, ch in enumerate(text):
            if ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
                if depth == 0:
                    break
        else:
            raise FormulaError("unbalanced parenthesis")
        key = text[1:i]
        n, used = _count(text[i + 1:], 0)
        rest = text[i + 1 + used:]
        if key not in POLYATOMIC_CATIONS:
            raise FormulaError("the leading group is not a known cation")
        if not rest:
            raise FormulaError("an ion is not a formula unit")
        return key, n, rest
    key = None
    for candidate in POLYATOMIC_CATIONS:
        if text.startswith(candidate):
            key = candidate
            break
    if key is not None:
        n, used = _count(text[len(key):], 0)
        rest = text[len(key) + used:]
        if not rest:
            raise FormulaError("an ion is not a formula unit")
        return key, n, rest
    m = _ELEMENT.match(text)
    if m is None:
        raise FormulaError("a formula must start with an element symbol")
    key = m.group(0)
    n, used = _count(text[m.end():], 0)
    rest = text[m.end() + used:]
    if key not in METALS:
        raise FormulaError("the leading element is not a metal")
    if not rest:
        raise FormulaError("an element is not a formula unit")
    return key, n, rest


def ion_counts(text):
    """Cations and anions supplied by one formula unit, as (n_cation, n_anion)."""

    s, charge = _prepare(resolve(text)[0])
    if charge:
        raise FormulaError("a charged species is not a formula unit")
    if "." in s:
        raise FormulaError("a hydrate is not one formula unit of ions")
    composition(s)
    key, cations, rest = _leading(s)
    kind, anion, anions = _split_term(rest)
    if kind == "element":
        if anion not in MONATOMIC_ANION_CHARGES:
            raise FormulaError("no charge is known for " + anion)
        anion_charge = MONATOMIC_ANION_CHARGES[anion]
    elif kind == "group":
        if anion not in POLYATOMIC_ANIONS:
            raise FormulaError("no charge is known for " + anion)
        anion_charge = POLYATOMIC_ANIONS[anion]
    else:
        if "." in rest:
            raise FormulaError("a hydrate is not one formula unit of ions")
        if anion not in POLYATOMIC_ANIONS:
            raise FormulaError("no charge is known for " + anion)
        anion_charge = POLYATOMIC_ANIONS[anion]
    if key in POLYATOMIC_CATIONS:
        cation_charge = POLYATOMIC_CATIONS[key]
    else:
        cation_charge = FIXED_CATION_CHARGES.get(key, 0)
    needed = anions * anion_charge
    if cation_charge:
        if cations * cation_charge != needed:
            raise FormulaError("the charges do not balance")
    else:
        if needed % cations != 0:
            raise FormulaError("the cation charge is not a whole number")
        cation_charge = needed // cations
        if cation_charge < 1 or cation_charge > 4:
            raise FormulaError("the cation charge is outside 1 to 4")
    return cations, anions


def resolve(text):
    """A typed argument as (formula, composition), accepting measured names."""

    if text is None:
        raise FormulaError("empty formula")
    try:
        return text.strip(), composition(text)
    except FormulaError as why:
        key = re.sub(r"[\s_\-]+", " ", text.strip().lower()).strip()
        if key in NAMES:
            return NAMES[key], composition(NAMES[key])
        raise why
