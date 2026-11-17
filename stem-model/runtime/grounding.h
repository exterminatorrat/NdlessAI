#ifndef GROUNDING_H
#define GROUNDING_H

/* Revised 2026-11-16: share numeric boundaries between grounding and repair. */

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

/* A span in text must not be part of a decimal, identifier or subscript.
 * Letters after a span remain legal for polynomial coefficients ("3x"). */
int grounding_number_boundary(const char *text, const char *start, int len);

/* Whether every argument of "CALL name a b ..." is in the question. */
int grounding_call_grounded(const char *call, const char *question);

#define GROUNDING_MAX_SPANS 24
#define GROUNDING_SPAN_LEN 24

/* One number as the question writes it, and where it sits. */
typedef struct {
	char text[GROUNDING_SPAN_LEN];
	int start;
	int end;
} grounding_span_t;

/*
 * Every number the question writes, in reading order.
 *
 * This is the span scan the repair pass and the argument mask both work from,
 * so a number the repair would accept and a number the mask would offer cannot
 * disagree about what counts as a number. Returns the count, at most `max`.
 */
int grounding_spans(const char *text, grounding_span_t *out, int max);

#endif
