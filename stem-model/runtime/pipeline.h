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
} pipeline_t;

enum {
	PIPELINE_OK = 0,
	PIPELINE_UNCERTAIN = 1,  /* call produced, but arguments not in the text */
	PIPELINE_ERR = -1
};

int pipeline_init(pipeline_t *p, const char *weights_path, int seq);

/*
 * The working for the call pipeline_answer just made: the formula, the numbers
 * substituted, and the answer. Returns 1 when a block was written, 0 when this
 * relation has no template (show the answer alone), -1 on truncation.
 */
int pipeline_steps(const char *call, const char *answer, char *out, int outlen);
void pipeline_free(pipeline_t *p);

/*
 * Answers one question. On PIPELINE_OK or PIPELINE_UNCERTAIN, `call` holds the
 * generated call and `answer` holds the solver's result (empty when uncertain).
 * Both buffers are always NUL-terminated on success.
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
 * call ("ff 0.4 500", or the relation's own name). Returns 0 when it answered,
 * 2 when it understood the question but had to refuse it, and 1 when the
 * question is neither, which is the caller's cue to use the model — or, on a
 * calculator with no weights file, to give up. `call` holds the call it made
 * in the first two cases.
 */
int pipeline_answer_offline(const char *question, char *answer, int answer_max,
                            char *call, int call_max);

/*
 * Exposed for testing: true when every argument in `call` is a number that
 * appears in `question`, allowing for implicit polynomial coefficients.
 */
int pipeline_args_grounded(const char *call, const char *question);

#endif
