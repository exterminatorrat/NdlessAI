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

typedef struct {
	model_t model;
	bpe_tok tok;
	qstate_t qs;
	qwork_t qw;
	int32_t *logits;
	int seq;
	int ready;
} pipeline_t;

enum {
	PIPELINE_OK = 0,
	PIPELINE_UNCERTAIN = 1,  /* call produced, but arguments not in the text */
	PIPELINE_ERR = -1
};

int pipeline_init(pipeline_t *p, const char *weights_path, int seq);
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
 * Exposed for testing: true when every argument in `call` is a number that
 * appears in `question`, allowing for implicit polynomial coefficients.
 */
int pipeline_args_grounded(const char *call, const char *question);

#endif
