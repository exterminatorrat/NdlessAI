#include <stdlib.h>
#include <string.h>

#include "callmask.h"
#include "pipeline.h"
#include "solvers.h"

#define PROMPT_SUFFIX " <sep>"
#define MAX_GEN 20
#define MAX_PROMPT 256
#define MAX_VOCAB 1024

static int argmax32(const int32_t *v, int n)
{
	int best = 0;

	for (int i = 1; i < n; i++)
		if (v[i] > v[best])
			best = i;
	return best;
}

static int ids_to_text(const bpe_tok *tok, const uint16_t *ids, int n,
                       char *out, int max)
{
	int used = 0;

	out[0] = '\0';
	for (int i = 0; i < n; i++) {
		char part[64];
		int got = bpe_token_bytes(tok, ids[i], part, (int)sizeof part);

		if (got < 0 || used + got + 1 > max)
			return -1;
		memcpy(out + used, part, (size_t)got);
		used += got;
	}
	out[used] = '\0';
	return used;
}

/*
 * Constrained decoding: the mask decides which tokens the next step may pick,
 * so a call the parser would reject is never generated in the first place.
 * Masked entries become INT32_MIN rather than being skipped, so argmax32 stays
 * the only chooser and the model's own ranking decides among the legal ones.
 *
 * Only the header is constrained. Forcing the argument count, or the end of a
 * number, would need a completion signal the runtime does not have, and
 * guessing it would truncate valid values.
 */
static void mask_logits(pipeline_t *p, const char *sofar)
{
	unsigned char allow[MAX_VOCAB];

	/* A vocabulary outside this buffer decodes unconstrained, exactly as it
	 * did before the mask existed. Masking is an improvement, never a
	 * precondition for answering, so it must not turn into an outage. */
	if (p->model.vocab <= 0 || p->model.vocab > MAX_VOCAB)
		return;
	callmask_next(sofar, allow, p->model.vocab);
	for (int i = 0; i < p->model.vocab; i++)
		if (!allow[i])
			p->logits[i] = INT32_MIN;
}

static int run_call_text(const char *call, char *answer, int answer_max)
{
	char work[256];
	char *parts[10];
	int n = 0;
	const char *args[8];
	size_t len = strlen(call);

	if (len + 1 > sizeof work)
		return -1;
	memcpy(work, call, len + 1);

	for (size_t i = 0; i < len && n < 10;) {
		while (i < len && work[i] == ' ')
			work[i++] = '\0';
		if (i >= len)
			break;
		parts[n++] = work + i;
		while (i < len && work[i] != ' ')
			i++;
	}
	if (n < 2 || strcmp(parts[0], "CALL") != 0)
		return -1;
	if (n - 2 > 8)
		return -1;
	for (int i = 0; i < n - 2; i++)
		args[i] = parts[i + 2];
	return solvers_solve(parts[1], args, n - 2, answer, answer_max);
}

/*
 * Every argument must be a number that appears in the question. The model's
 * dominant failure on unseen wording is copying a plausible but wrong number,
 * and those show up as arguments with no counterpart in the text. Refusing is
 * better than answering: a wrong number is indistinguishable from a right one.
 *
 * Two allowances, both forced by how the questions are written:
 *
 *  - 0 and 1 are always accepted. Polynomial questions render coefficients
 *    implicitly ("x^3" not "1x^3", "2x^2 - 3" with no x term), so those values
 *    are legitimately absent from the text. The cost is that a wrong 0 or 1
 *    passes; the benefit is that correct answers are not thrown away.
 *  - Signs are matched, not ignored. The text writes "2x - 1" while the
 *    argument is "-1", so a negative argument looks for a minus before the
 *    digits, and a positive one requires an occurrence with no minus. Without
 *    this, a model that flips a sign still passes: "x^2 - 3x + 2" contains a
 *    3, so an argument of +3 looked grounded when the answer needed -3.
 */
static int is_digit(char c)
{
	return c >= '0' && c <= '9';
}

static int arg_in_text(const char *arg, const char *question)
{
	int negative = (*arg == '-');
	const char *digits = negative ? arg + 1 : arg;
	size_t n = strlen(digits);

	if (!n)
		return 0;
	if (!strcmp(digits, "0") || !strcmp(digits, "1"))
		return 1;

	for (const char *p = question; (p = strstr(p, digits)) != NULL; p++) {
		const char *before = p;
		int has_minus = 0;

		if (is_digit(p[n]))
			continue;
		while (before > question && before[-1] == ' ')
			before--;
		if (before > question && before[-1] == '-')
			has_minus = 1;
		else if (before > question && is_digit(before[-1]))
			continue;
		if (has_minus == negative)
			return 1;
	}
	return 0;
}

int pipeline_args_grounded(const char *call, const char *question)
{
	char work[256];
	size_t len = strlen(call), i = 0;
	int word = 0;

	if (len + 1 > sizeof work)
		return 0;
	memcpy(work, call, len + 1);

	while (i <= len) {
		char *start;

		while (i < len && work[i] == ' ')
			work[i++] = '\0';
		if (i >= len)
			break;
		start = work + i;
		while (i < len && work[i] != ' ')
			i++;
		if (i < len)
			work[i++] = '\0';
		if (word >= 2 && !arg_in_text(start, question))
			return 0;
		word++;
	}
	return 1;
}

int pipeline_init(pipeline_t *p, const char *weights_path, int seq)
{
	memset(p, 0, sizeof(*p));
	if (model_load(weights_path, &p->model) < 0)
		return -1;
	p->model.seq = seq;
	p->seq = seq;
	if (bpe_init(&p->tok) < 0)
		goto fail;
	if (qstate_init(&p->qs, &p->model, seq) < 0)
		goto fail;
	if (qwork_alloc(&p->qw, &p->model) < 0)
		goto fail;
	p->logits = malloc((size_t)p->model.vocab * sizeof *p->logits);
	if (!p->logits)
		goto fail;
	p->ready = 1;
	return 0;

fail:
	pipeline_free(p);
	return -1;
}

void pipeline_free(pipeline_t *p)
{
	if (!p->ready && !p->logits)
		return;
	free(p->logits);
	p->logits = NULL;
	qwork_free(&p->qw);
	qstate_free(&p->qs);
	model_free(&p->model);
	p->ready = 0;
}

int pipeline_answer(pipeline_t *p, const char *question,
                    char *answer, int answer_max,
                    char *call, int call_max)
{
	uint16_t ids[MAX_PROMPT];
	char prompt[MAX_PROMPT + 16];
	int n, pos = 0, gen = 0;
	size_t qlen = strlen(question);

	if (!p->ready)
		return PIPELINE_ERR;
	if (qlen + sizeof(PROMPT_SUFFIX) > sizeof prompt)
		return PIPELINE_ERR;
	memcpy(prompt, question, qlen);
	memcpy(prompt + qlen, PROMPT_SUFFIX, sizeof(PROMPT_SUFFIX));

	n = bpe_encode(&p->tok, prompt, ids, MAX_PROMPT);
	if (n <= 0 || n >= p->seq)
		return PIPELINE_ERR;

	for (int i = 0; i < n; i++)
		if (forward_q(&p->model, &p->qs, &p->qw, ids[i], pos++, p->logits) < 0)
			return PIPELINE_ERR;

	for (int i = 0; i < MAX_GEN && pos < p->seq; i++) {
		char sofar[MAX_PROMPT + 1];
		int next;

		if (ids_to_text(&p->tok, ids + n, gen, sofar, (int)sizeof sofar) < 0)
			return PIPELINE_ERR;
		mask_logits(p, sofar);
		next = argmax32(p->logits, p->model.vocab);

		if (next == 258 || next == 256)
			break;
		if (n + gen + 1 >= MAX_PROMPT)
			break;
		ids[n + gen++] = (uint16_t)next;
		if (forward_q(&p->model, &p->qs, &p->qw, next, pos++, p->logits) < 0)
			break;
	}

	if (ids_to_text(&p->tok, ids + n, gen, call, call_max) < 0)
		return PIPELINE_ERR;
	if (run_call_text(call, answer, answer_max) < 0)
		return PIPELINE_ERR;
	if (!pipeline_args_grounded(call, question)) {
		answer[0] = '\0';
		return PIPELINE_UNCERTAIN;
	}
	return PIPELINE_OK;
}
