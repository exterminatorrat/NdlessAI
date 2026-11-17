#ifndef QUANTITY_H
#define QUANTITY_H

/*
 * Revised 2026-11-17: unitless zero does not imply a shared scaling unit.
 *
 * The unit a question wrote after each number, checked against the unit the
 * relation's slot expects.
 *
 * The grounding rule proves a number came from the sentence; it cannot say the
 * number is in the unit the formula needs. "A 10 kg ball moves at 72 km/h"
 * grounds "CALL kinetic_energy 10 72" perfectly and answers 25,920 J, which is
 * wrong by a factor of 13 and looks right. And when the model swaps two
 * arguments, "CALL kinetic_energy 72 10", grounding passes that too. Both are
 * the right-formula, wrong-numbers failure that dominates on hand-written
 * questions, and both are visible from the units alone.
 *
 * So every argument is matched to the number it was copied from, and the unit
 * written after that number is compared with the slot's unit from reltable.h:
 *
 *   same unit                 the argument is left exactly as decoded
 *   same kind, convertible    rewritten in the slot's unit, exactly: km/h to
 *                             m/s is 5/18, so 72 becomes 20
 *   different kind            the call is refused: a mass in a speed slot is
 *                             an argument in the wrong place, not a value
 *   same kind, no exact rate  refused too: degrees and radians differ by pi,
 *                             eV and J by a constant this engine cannot hold
 *
 * Anything this cannot read is left alone, because the cost of a false refusal
 * is a question the student could have had answered: a slot with no declared
 * unit, a number with no recognised unit after it, a number the question
 * writes twice with different units (or once without one), an argument
 * that is not in the text at all, which grounding handles, and a 0 or 1 next
 * to a unit of another kind, because grounding lets those two values stand
 * for a number the text never writes ("one mole in 1 kg"), and a number whose
 * non-zero partner of the same kind carries no unit of its own, because "2 and 8 kg"
 * writes one unit for both and converting only the 8 changes the answer.
 * A unitless zero does not block multiplicative conversions: zero heat or
 * speed stays zero under scaling. Absolute temperatures keep the fallback,
 * because their conversions may include an offset.
 *
 * Numbers written in one unit are converted together, with one shared
 * rounding, so their ratios survive: 3 lb and 7 lb stay 3 to 7. Celsius and
 * Fahrenheit are the exception, converted exactly and rounded one by one,
 * because their offset makes a shared rounding inaccurate near absolute zero.
 *
 * A slot may accept alternatives ("M|mol" for a ratio of base to acid); its
 * partners in the same alternative slot must then be written in one kind.
 *
 * Temperature differences are deliberately not declared on any slot. "A 20
 * degree rise" is neither an angle nor a Celsius reading, and converting it
 * with the 273.15 offset would be wrong, so those slots carry no unit.
 */

enum {
	QUANTITY_SAME = 0,        /* nothing to change; `out` holds the call */
	QUANTITY_CONVERTED = 1,   /* `out` holds the call in the slots' units */
	QUANTITY_MISMATCH = -1    /* an argument's unit cannot fill its slot */
};

/*
 * Checks "CALL name a b ..." against `question`. `out` always holds a call:
 * the converted one on QUANTITY_CONVERTED, the original otherwise. A call
 * that does not parse, or names an unknown relation, is QUANTITY_SAME; the
 * solver is the place that refuses it.
 */
int quantity_check_call(const char *call, const char *question,
                        char *out, int outlen);

/*
 * The canonical name of the unit written at `end`, the first character after a
 * number, or NULL when none is recognised. When `span` is not NULL it receives
 * how many characters from `end` the unit took. Exposed for tests.
 */
const char *quantity_unit_after(const char *end, int *span);

/*
 * `value` in unit `from` written in unit `to`, both canonical or any spelling,
 * as the decimal quantity_check_call would pass to the solver. Returns 0 when
 * exact, 1 when rounded (quantity.c says to how many digits, and why), -1 when
 * the two cannot be converted or the value cannot be written. Exposed for
 * tests.
 */
int quantity_convert(const char *value, const char *from, const char *to,
                     char *out, int outlen);

/*
 * The canonical name of the n-th other unit of the same kind as `unit` that it
 * converts to exactly, or NULL past the last one. Exposed for tests.
 */
const char *quantity_unit_alternative(const char *unit, int n);

/* Whether `name` is a unit this module knows, under any spelling. */
int quantity_unit_known(const char *name);

#endif
