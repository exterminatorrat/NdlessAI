#ifndef SHORTHAND_H
#define SHORTHAND_H

/*
 * A call the student types instead of describing: "fri 0.4 500" is
 * "CALL friction_force 0.4 500", and so is the relation's own name. Shorter
 * forms are the initials of the name's parts ("kv" for kinematic_v, "ov" for
 * ohm_v) when they are unique, or as much of the name as it takes to be
 * unique, never fewer than three letters.
 *
 * Typing a sentence on a calculator keypad is the slowest part of using this
 * program, and it is also the part that can be wrong: the decoder has to read
 * the words back. A shorthand skips the decoder entirely, so it is exact and
 * instant, and it costs no model. The names it accepts are the ones the answer
 * pane already prints on its CALL line, so the screen teaches the syntax.
 *
 * Returns 0 for a built call, 1 when the text names a relation but its
 * arguments do not fit, and -1 when the text is not a shorthand at all. In
 * both of the first two cases `call` holds the call it understood, so the
 * screen can show what it tried.
 */
int shorthand_build(const char *text, char *call, int call_max);

#endif
