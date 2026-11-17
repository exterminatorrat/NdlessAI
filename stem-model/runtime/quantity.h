#ifndef QUANTITY_H
#define QUANTITY_H

/*
 * The unit a question writes after a number.
 *
 * The grounding rule proves a number came from the sentence; it cannot say
 * what unit it was written in, and the unit is what separates "10 kg at
 * 72 km/h" from a call that answers 25,920 J. This half reads the unit: the
 * longest spelling the table knows, read whole or not at all, so "m/s^2" is
 * never "m/s" and "metres per minute" is never metres. Checking a call's
 * arguments against the units their slots declare builds on it.
 */

/*
 * The canonical name of the unit written at `end`, the first character after a
 * number, or NULL when none is recognised. When `span` is not NULL it receives
 * how many characters from `end` the unit took. Exposed for tests.
 */
const char *quantity_unit_after(const char *end, int *span);

/* Whether `name` is a unit this module knows, under any spelling. */
int quantity_unit_known(const char *name);

#endif
