"""Check that every step template reproduces the answer its solver returns.

A step template is display text and arithmetic at the same time: `expr` is the
formula with one '@' per argument. This evaluates it on rows the relation's own
generator produces and compares it against the string the solver prints, so a
template that looks plausible but is wrong fails here instead of showing
confident working to a student.

    python3 check_steps.py            # check everything with a template
    python3 check_steps.py --list     # show coverage instead

Exit code is 1 when a template disagrees with its solver, or when a template
has the wrong number of placeholders.
"""

import argparse
import math
import random
import re
import sys

from relations import RELATIONS

ROWS = 25
REL_TOL = 5e-4  # answers render to four significant digits

NUM = re.compile(r"-?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?")
FRAC = re.compile(r"(-?\d+)/(\d+)")

# The namespace an `expr` may use. Trigonometry is in DEGREES, matching the
# relations themselves, and `ln`/`log10` are named rather than imported so a
# template reads like the formula on the paper. Nothing else is available: an
# expr is arithmetic, not code.
FUNCS = {
    "abs": abs,
    "ln": math.log,
    "log10": math.log10,
    "exp": math.exp,
    "sqrt": math.sqrt,
    "sin": lambda d: math.sin(math.radians(d)),
    "cos": lambda d: math.cos(math.radians(d)),
    "tan": lambda d: math.tan(math.radians(d)),
    "asin": lambda x: math.degrees(math.asin(x)),
    "acos": lambda x: math.degrees(math.acos(x)),
    "pi": math.pi,
}


def first_number(text):
    """The answer as a number: exact fractions are values, not numerators."""
    flat = text.replace(",", "")
    m = FRAC.match(flat.strip())
    if m:
        return int(m.group(1)) / int(m.group(2))
    m = NUM.search(flat)
    return float(m.group(0)) if m else None


PLACEHOLDER = re.compile(r"@(\d+)")


def render_expr(expr, args):
    """Substitute indexed '@1'..'@n' holes with the arguments they name."""
    seen = {int(m.group(1)) for m in PLACEHOLDER.finditer(expr)}
    if seen and (min(seen) < 1 or max(seen) > len(args)):
        raise ValueError(f"placeholders {sorted(seen)} for {len(args)} arguments")

    def rep(m):
        return f"({args[int(m.group(1)) - 1]})"

    return PLACEHOLDER.sub(rep, expr)


def check_one(name, rel):
    rng = random.Random(0xC0DE)
    for _ in range(ROWS):
        args, _question = rel.gen(rng)
        try:
            rendered = render_expr(rel.step["expr"], args)
        except ValueError as e:
            return f"bad template: {e}"
        try:
            want_expr = eval(rendered.replace("^", "**"),
                             {"__builtins__": {}}, FUNCS)
        except Exception as e:
            return f"template does not evaluate ({rendered}): {e}"
        answer = rel.solve(*args)
        want_solver = first_number(answer)
        if want_solver is None:
            return f"solver answer {answer!r} has no number to compare against"
        # Trigonometry at a right angle lands on 1e-17 rather than 0, so an
        # absolute floor is as necessary as the relative one.
        ok = abs(want_expr - want_solver) <= max(REL_TOL * abs(want_expr), 1e-6)
        if not ok:
            return (f"mismatch for {args}: template says {want_expr:g}, "
                    f"solver says {answer!r} ({rendered})")
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", action="store_true")
    args = ap.parse_args()

    with_steps = {n: r for n, r in RELATIONS.items() if r.step}
    without = sorted(n for n, r in RELATIONS.items() if not r.step)

    if args.list:
        print(f"{len(with_steps)} of {len(RELATIONS)} relations show working")
        for n in sorted(with_steps):
            print(f"  {n:24} {with_steps[n].step.get('line', '')}")
        print("\nno working shown:")
        for n in without:
            print(f"  {n}")
        return 0

    bad = 0
    for name in sorted(with_steps):
        rel = with_steps[name]
        missing = [k for k in ("line", "expr") if not rel.step.get(k)]
        if missing:
            print(f"FAIL {name}: missing {', '.join(missing)}")
            bad += 1
            continue
        err = check_one(name, rel)
        if err:
            print(f"FAIL {name}: {err}")
            bad += 1

    print(f"\n{len(with_steps) - bad}/{len(with_steps)} templates agree with their solvers")
    if bad:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
