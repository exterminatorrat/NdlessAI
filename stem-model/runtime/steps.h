#ifndef STEPS_H
#define STEPS_H

/*
 * Show the working: the formula, the numbers in it, and the answer.
 *
 * A relation that carries a step template produces
 *
 *     Q = m * L
 *     = 25 * 334
 *     = 8350 J
 *
 * from its name, its arguments and the answer the solver already computed.
 * The templates live in steptable.h, generated from relations/ by
 * export_steps.py, and check_steps.py proves each one reproduces its solver's
 * answer - so this file formats, it never computes.
 *
 * ASCII only. libndls converts the text to UTF-16 through ascii2utf16, so a
 * multiplication sign would arrive on screen as mojibake.
 */

/*
 * Writes the three-line block into out. `answer` is appended as the last line,
 * so the caller can show the result of this call instead of the bare answer.
 *
 * Returns 1 when a block was written, 0 when the relation has no template (or
 * the name is unknown, which the caller then answers without working), and -1
 * when the block did not fit in out, which is truncated but still terminated.
 */
int steps_render(const char *name, const char *const *args, int nargs,
                 const char *answer, char *out, int outlen);

#endif
