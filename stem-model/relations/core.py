import random

import sympy as sp


class Relation:
    def __init__(self, name, subject, params, gen, solve, exact):
        self.name = name
        self.subject = subject
        self.params = tuple(params)
        self.gen = gen
        self.solve = solve
        self.exact = exact


RELATIONS = {}


def relation(name, subject, params, exact=True):
    def register(cls):
        RELATIONS[name] = Relation(name, subject, params, cls.gen, cls.solve, exact)
        return cls

    return register


def fmt(value):
    if isinstance(value, (int, sp.Integer)):
        return str(int(value))
    if isinstance(value, sp.Rational):
        return str(value)
    return dec(value)


def _exact(value):
    if isinstance(value, (int, sp.Integer)):
        return sp.Rational(int(value))
    if isinstance(value, sp.Rational):
        return sp.Rational(value)
    if isinstance(value, sp.Float):
        return sp.Rational(str(value))
    if isinstance(value, float):
        return sp.Rational(repr(value))
    return sp.Rational(str(sp.N(value, 20)))


def dec(value):
    return f"{float(value):.4g}"


def dec_exact(value):
    """Four significant digits, round half to even, matching dec4_render.

    Use this when the answer is an exact rational of the decimal inputs, as
    with a physical constant like 9.81. There the C renderer rounds the exact
    rational while %.4g rounds the nearest double, and the two disagree on
    ties: 15*3*9.81 is exactly 441.45, so the device shows 441.4 and %.4g
    gives 441.5. Rounding the exact rational here keeps them identical.

    Do not use this for irrational results such as a sine or a square root.
    There the double is the better approximation of the true value and
    matches the device's integer approximation more often.
    """

    r = _exact(value)
    if r == 0:
        return "0"
    neg = r < 0
    r = abs(r)
    e = 0
    while r >= 10:
        r /= 10
        e += 1
    while r < 1:
        r *= 10
        e -= 1
    x = r * 10000
    n = int(x)
    rem = x - n
    d = [int(c) for c in str(n).rjust(5, "0")]
    up = 0
    if d[4] > 5 or (d[4] == 5 and rem > 0):
        up = 1
    elif d[4] == 5:
        up = d[3] & 1
    if up:
        v = int("".join(str(d[i]) for i in range(4))) + 1
        if v >= 10000:
            v //= 10
            e += 1
        d = [int(c) for c in str(v).rjust(4, "0")] + [0]
    else:
        d[4] = 0
    if e >= 4 or e <= -5:
        fz = 3
        while fz > 0 and d[fz] == 0:
            fz -= 1
        s = str(d[0])
        if fz > 0:
            s += "." + "".join(str(d[i]) for i in range(1, fz + 1))
        s += "e" + ("-" if e < 0 else "+") + str(abs(e)).rjust(2, "0")
    elif e >= 0:
        s = "".join(str(d[i]) for i in range(e + 1))
        fz = 3
        while fz > e and d[fz] == 0:
            fz -= 1
        if fz > e:
            s += "." + "".join(str(d[i]) for i in range(e + 1, fz + 1))
    else:
        fz = 3
        while fz > 0 and d[fz] == 0:
            fz -= 1
        s = "0." + "0" * (-e - 1) + "".join(str(d[i]) for i in range(fz + 1))
    return ("-" if neg else "") + s


def sig_render(value, n):
    """Round an exact decimal to n significant figures, plain decimal form.

    Zeros that carry magnitude are kept, so 12345 to 3 figures is 12300, while
    zeros after the decimal point are dropped, so 3.14159 to 4 is 3.142.
    Rounding is half to even on the exact value, matching the C renderer.
    """

    r = _exact(value)
    if r == 0:
        return "0"
    neg = r < 0
    r = abs(r)
    num, den = r.p, r.q
    if num >= den:
        e = len(str(num // den)) - 1
    else:
        e = 0
        t = num
        while t < den:
            t *= 10
            e -= 1
    p = e - n + 1
    if p >= 0:
        a, b = num, den * 10 ** p
    else:
        a, b = num * 10 ** (-p), den
    q, rem = divmod(a, b)
    if 2 * rem > b or (2 * rem == b and q % 2):
        q += 1
    if p >= 0:
        s = str(q * 10 ** p)
    else:
        k = -p
        digits = str(q)
        if len(digits) <= k:
            digits = "0" * (k - len(digits) + 1) + digits
        s = (digits[:-k] + "." + digits[-k:]).rstrip("0").rstrip(".")
    return ("-" if neg else "") + s


def poly(coeffs, var="x"):
    terms = []
    deg = len(coeffs) - 1
    for i, c in enumerate(coeffs):
        d = deg - i
        if c == 0:
            continue
        mag = abs(c)
        if d == 0:
            body = str(mag)
        elif d == 1:
            body = ("" if mag == 1 else str(mag)) + var
        else:
            body = ("" if mag == 1 else str(mag)) + f"{var}^{d}"
        if not terms:
            terms.append(("-" if c < 0 else "") + body)
        else:
            terms.append((" - " if c < 0 else " + ") + body)
    return "".join(terms) or "0"


def lin2(a, b, c):
    """Render one equation of a two-variable system, e.g. "4x - 2y = -16"."""

    def term(coef, var):
        if coef == 0:
            return None
        mag = abs(coef)
        return (("" if mag == 1 else str(mag)) + var, coef < 0)

    parts = [p for p in (term(a, "x"), term(b, "y")) if p]
    if not parts:
        body = "0"
    else:
        body = ("-" if parts[0][1] else "") + parts[0][0]
        for text, neg in parts[1:]:
            body += (" - " if neg else " + ") + text
    return f"{body} = {c}"


def amp(c):
    """Amplitude prefix for a sinusoid: "5 ", "-", or "" for a magnitude of one."""

    if c == 0:
        return ""
    mag = abs(c)
    return ("-" if c < 0 else "") + ("" if mag == 1 else str(mag) + " ")
