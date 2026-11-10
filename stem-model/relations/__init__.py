"""Relation registry, split by subject.

core.py holds the shared helpers and the @relation decorator; each subject
module registers its own relations on import, so `import relations` sees all
of them exactly as it did when this was a single file.
"""

from .core import (RELATIONS, Relation, _exact, amp, dec, dec_exact, fmt,
                   lin2, poly, relation, sig_render)

from . import (  # noqa: F401  (imported for their @relation side effects)
    algebra,
    calculus,
    chemistry,
    em,
    logs,
    physics,
    precalc,
    thermo,
    trig,
    units,
)
