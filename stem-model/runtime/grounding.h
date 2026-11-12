#ifndef GROUNDING_H
#define GROUNDING_H

/*
 * Is a number the question actually said?
 *
 * Every argument of an answer has to come from the sentence: a model that
 * copies a plausible number the question never mentioned is the failure this
 * whole project is built around, and the screen refuses rather than answering
 * when it happens. The rule is shared — the decoder's repair pass and the
 * pipeline's refusal both ask this question — so it lives here once.
 *
 * Two allowances, both forced by how questions are written:
 *
 *  - 0 and 1 are always accepted. Polynomial questions render coefficients
 *    implicitly ("x^3" not "1x^3", "2x^2 - 3" with no x term), so those values
 *    are legitimately absent from the text. The cost is that a wrong 0 or 1
 *    passes; the benefit is that correct answers are not thrown away.
 *  - Signs are matched, not ignored. The text writes "2x - 1" while the
 *    argument is "-1", so a negative argument looks for a minus before the
 *    digits, and a positive one requires an occurrence with no minus.
 */

/* Whether `arg` appears in `question` as a number. */
int grounding_arg_in_text(const char *arg, const char *question);

/* Whether every argument of "CALL name a b ..." is in the question. */
int grounding_call_grounded(const char *call, const char *question);

#endif
