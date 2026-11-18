#ifndef PIPELINE_H
#define PIPELINE_H

#include <stdint.h>

#include "bpe.h"
#include "fixed.h"
#include "infer.h"

/*
 * The whole on-device path in one object:
 *
 *   question text -> BPE -> fixed-point forward -> greedy decode
 *                 -> call string -> C solver -> answer text
 *
 * Shared by the host driver and the calculator app so both run identical code.
 * No float in the arithmetic and no libm in the per-token loop.
 */

/*
 * How many relations the decode keeps from the step that names one. The
 * graphing screen shows the winner plus two runners-up, so a fixed three keeps
 * the whole feature in the object and off the heap.
 */
#define PIPELINE_ALTS 3

typedef struct {
	uint16_t token;
	int32_t margin;   /* logit gap to the winner, >= 0 */
} pipeline_alt_t;

typedef struct {
	model_t model;
	bpe_tok tok;
	qstate_t qs;
	qwork_t qw;
	int32_t *logits;
	int seq;
	int ready;
	int n_alts;
	pipeline_alt_t alts[PIPELINE_ALTS];
	/*
	 * How close the runner-up relation may come before the call is refused
	 * rather than printed, in the same units as pipeline_alt_t.margin. Zero
	 * disables the check, which is what every path that answers without the
	 * model does anyway: nothing was weighed, so there is nothing to doubt.
	 * The value is a calibrated constant in pipeline.c, and it is a field so a
	 * test can sweep it.
	 */
	int32_t margin_min;
	/*
	 * Whether the decode may write a second call after a ';' and so produce a
	 * chain. Zero by default, and that is a measurement rather than caution:
	 * no corpus in the tree has a chain target, so the weights have never been
	 * shown a ';' in a call and cannot prefer one. With this set the mask
	 * stops blocking the shape (see callmask.h); the corpus is what makes it
	 * likely. A test sets it to prove the rest of the path handles what the
	 * mask would let through.
	 */
	int allow_chain;
} pipeline_t;

enum {
	PIPELINE_OK = 0,
	/*
	 * A call was produced and refused: its arguments are not all numbers the
	 * question wrote, or the relation it named was too close to its runner-up
	 * for the decode to have meant it.
	 */
	PIPELINE_UNCERTAIN = 1,
	/*
	 * The text was arithmetic and nothing else, and the exact engine's node
	 * pool could not hold it — a flat sum of more than 64 terms, for example.
	 * It is refused rather than handed to the decoder, which would read it as
	 * a word problem and answer with invented numbers; the screen can say so in
	 * words because it is a different thing from a model failure.
	 */
	PIPELINE_TOO_LONG = 2,
	/*
	 * The text was arithmetic and the exact engine has no value for it: a
	 * division by zero, the root of a negative, the logarithm of zero, or a
	 * result outside the Q16.16 range it computes in. Bounded as it is, the
	 * engine used to saturate those to 32768 and print it, which is the one
	 * thing a calculator must never do with a question that has no answer.
	 */
	PIPELINE_NO_VALUE = 3,
	/*
	 * A call was produced and every argument is a number the question wrote,
	 * but a unit cannot fill its slot: a mass where a speed belongs is an
	 * argument in the wrong place, and the answer would be a plausible number
	 * for a question nobody asked. The screen refuses in words instead.
	 */
	PIPELINE_UNITS = 4,
	/*
	 * A call was produced, every argument is a number the question wrote, and
	 * the units were converted exactly — then the solver refused the converted
	 * numbers. A conversion legitimately changes the scale of a written number:
	 * "450 hours" becomes 1620000 s, and "1 km/h" becomes 0.2778 m/s. The exact
	 * arithmetic keeps its numerators and denominators inside fixed limits, so
	 * a number that grew, or a value that needed decimals to survive the
	 * conversion, can land outside them. Measured on a sweep of single
	 * conversions: 1013 of 6156 converted calls, 450 of them carrying a
	 * decimal.
	 *
	 * The question was read and the conversion was right; it is the converted
	 * number the build cannot compute with, which is why this is its own code
	 * and not PIPELINE_NO_VALUE: no answer is missing and nothing is out of
	 * range in Q16.16, and the next step a student needs is different.
	 */
	PIPELINE_UNITS_LIMIT = 5,
	PIPELINE_ERR = -1
};

/* pipeline_answer_offline's own vocabulary, distinct from the result codes
 * above because "not mine" is not a verdict about the question. */
#define PIPELINE_OFFLINE_ANSWERED 0
#define PIPELINE_OFFLINE_NOT_MINE 1
#define PIPELINE_OFFLINE_REFUSED 2
#define PIPELINE_OFFLINE_TOO_LONG 3
#define PIPELINE_OFFLINE_NO_VALUE 4

int pipeline_init(pipeline_t *p, const char *weights_path, int seq);

/*
 * The working for the call pipeline_answer just made: the formula, the numbers
 * substituted, and the answer. Returns 1 when a block was written, 0 when this
 * relation has no template (show the answer alone), -1 on truncation.
 */
int pipeline_steps(const char *call, const char *answer, char *out, int outlen);
void pipeline_free(pipeline_t *p);

/*
 * Answers one question. On PIPELINE_OK, PIPELINE_UNCERTAIN or PIPELINE_UNITS,
 * `call` holds the generated call and `answer` holds the solver's result (empty
 * unless OK). Both buffers are always NUL-terminated on success.
 *
 * A number the question wrote in another unit of the same kind is converted
 * before the solver sees it, and `call` then holds the converted call, which
 * is the one the answer and its working came from: "10 kg at 72 km/h" answers
 * through "CALL kinetic_energy 10 20". See quantity.h.
 */
int pipeline_answer(pipeline_t *p, const char *question,
                    char *answer, int answer_max,
                    char *call, int call_max);

/*
 * The relations the last pipeline_answer weighed, best first: the one it used
 * with margin 0, then the runners-up and how far behind they were. The screen
 * draws the winner solid and the rest dimmed, so a near miss is visible instead
 * of silent.
 *
 * Returns the number of entries written, at most max. Zero means the model
 * never weighed a relation: typed arithmetic and the other closed forms answer
 * before inference runs, an unloaded pipeline cannot answer at all, and a
 * decode that failed before the header finished named nothing. The entries
 * always describe the answer just produced, never an earlier one.
 */
int pipeline_alternatives(const pipeline_t *p, pipeline_alt_t *out, int max);

/*
 * Answers text that is already an expression rather than a word problem, with
 * no model involved: "sqrt(50)+ln(2)" evaluates here in microseconds. Text
 * containing x has no single value and is refused, so it falls back to the
 * model path. Returns PIPELINE_OK on success, PIPELINE_ERR otherwise.
 */
int pipeline_typed_answer(const char *text, char *answer, int answer_max);

/*
 * Anything answerable with no model: typed arithmetic first, then a typed
 * call ("ff 0.4 500", or the relation's own name). Returns
 * PIPELINE_OFFLINE_ANSWERED when it answered, PIPELINE_OFFLINE_REFUSED when it
 * understood the question but had to refuse it, PIPELINE_OFFLINE_TOO_LONG and
 * PIPELINE_OFFLINE_NO_VALUE for the two kinds of arithmetic the exact engine
 * cannot answer (neither may become a model question), and
 * PIPELINE_OFFLINE_NOT_MINE when the question is neither, which is the
 * caller's cue to use the model — or, on a calculator with no weights file, to
 * give up. `call` holds the call it made in the first two cases.
 */
int pipeline_answer_offline(const char *question, char *answer, int answer_max,
                            char *call, int call_max);

/*
 * Exposed for testing: true when every argument in `call` is a number that
 * appears in `question`, allowing for implicit polynomial coefficients.
 */
int pipeline_args_grounded(const char *call, const char *question);

#endif
