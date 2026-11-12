#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expr.h"
#include "callmask.h"
#include "matrix.h"
#include "pipeline.h"
#include "plan.h"
#include "shorthand.h"
#include "solvers.h"
#include "steps.h"

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

/*
 * The relation is one token, chosen at the step after "CALL " and already
 * narrowed to the relation ids by the mask. Keeping the top three rather than
 * only the argmax lets the graphing screen draw the runners-up dimmed, so a
 * near miss is visible instead of silent.
 *
 * Ties keep the token seen first, which is argmax32's own rule, so alts[0] is
 * always the token the decode actually emitted and its margin is exactly zero.
 * Masked entries are INT32_MIN, not scores: skipping them keeps a masked token
 * out of the list and keeps the margin subtraction from overflowing.
 */
static void keep_alts(pipeline_t *p)
{
	struct {
		uint16_t token;
		int32_t logit;
	} best[PIPELINE_ALTS];
	int n = 0;

	for (int i = 0; i < p->model.vocab; i++) {
		int32_t v = p->logits[i];
		int at;

		if (v == INT32_MIN)
			continue;
		for (at = n; at > 0 && v > best[at - 1].logit; at--)
			;
		if (at == PIPELINE_ALTS)
			continue;
		if (n < PIPELINE_ALTS)
			n++;
		for (int j = n - 1; j > at; j--)
			best[j] = best[j - 1];
		best[at].token = (uint16_t)i;
		best[at].logit = v;
	}

	for (int i = 0; i < n; i++) {
		p->alts[i].token = best[i].token;
		p->alts[i].margin = best[0].logit - best[i].logit;
	}
	p->n_alts = n;
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

typedef struct {
	const char *name;
	const char *args[8];
	int nargs;
} call_t;

/* Splits "CALL name a b c" into its name and arguments, in place in work. */
static int parse_call(const char *call, char *work, size_t worklen, call_t *c)
{
	char *parts[10];
	int n = 0;
	size_t len = strlen(call);

	if (len + 1 > worklen)
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
	c->name = parts[1];
	c->nargs = n - 2;
	for (int i = 0; i < c->nargs; i++)
		c->args[i] = parts[i + 2];
	return 0;
}

/* A chain of calls is executed as a plan; a single call goes straight to the
 * solver. Both end here so the model path and the typed path cannot diverge. */
static int run_call_text(const char *call, char *answer, int answer_max)
{
	plan_t plan;
	char work[256];
	call_t c;

	if (plan_is(call)) {
		if (plan_execute(call, &plan) < 0)
			return -1;
		snprintf(answer, (size_t)answer_max, "%s",
		         plan.step[plan.n - 1].answer);
		return 0;
	}
	if (parse_call(call, work, sizeof work, &c) < 0)
		return -1;
	return solvers_solve(c.name, c.args, c.nargs, answer, answer_max);
}

int pipeline_steps(const char *call, const char *answer, char *out, int outlen)
{
	char work[256];
	call_t c;

	if (out == NULL || outlen < 1)
		return -1;
	out[0] = 0;
	if (call == NULL || call[0] == 0)
		return 0;
	if (plan_is(call)) {
		plan_t plan;

		if (plan_execute(call, &plan) < 0)
			return 0;
		return plan_render(&plan, out, outlen);
	}
	if (parse_call(call, work, sizeof work, &c) < 0)
		return 0;
	return steps_render(c.name, c.args, c.nargs, answer, out, outlen);
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

int pipeline_typed_answer(const char *text, char *answer, int answer_max)
{
	gexpr_t e;
	char err[48];

	/* Linear algebra is not an expression either: brackets, ';' and ',' have
	 * no arithmetic meaning, so det/inv/rank/solve are recognised on the raw
	 * text before anything is parsed as a formula. */
	if (matrix_typed_answer(text, answer, (size_t)answer_max) == 0)
		return PIPELINE_OK;
	/* A derivative request is not an expression: "d/dx" carries a slash the
	 * parser would read as division, so it is recognised on the raw text and
	 * answered with the derivative before any arithmetic is attempted. */
	if (gexpr_derivative_answer(text, answer, answer_max) == 0)
		return PIPELINE_OK;
	/* A definite integral is not an expression either: the parser has no
	 * "integrate" or "int" function, so the form is recognised first. */
	if (gexpr_integral_answer(text, answer, answer_max) == 0)
		return PIPELINE_OK;
	/* A series is a form too: "series" is not a function the parser knows, and
	 * the answer is an expression rather than a value. */
	if (gexpr_series_answer(text, answer, answer_max) == 0)
		return PIPELINE_OK;
	/* A differential equation is its own form: y is not an input the graph or
	 * a plain expression accepts. */
	if (gexpr_ode_answer(text, answer, answer_max) == 0)
		return PIPELINE_OK;
	/* The analytical form of the same equation, as an expression. */
	if (gexpr_desolve_answer(text, answer, answer_max) == 0)
		return PIPELINE_OK;
	if (!text || gexpr_parse(text, &e, err, sizeof err) < 0)
		return PIPELINE_ERR;
	if (gexpr_has_x(&e) || gexpr_has_y(&e))
		return PIPELINE_ERR;
	if (gexpr_format(gexpr_eval(&e, 0), answer, answer_max) < 0)
		return PIPELINE_ERR;
	return PIPELINE_OK;
}

int pipeline_answer_offline(const char *question, char *answer, int answer_max,
                            char *call, int call_max)
{
	char guess[64];
	int rc;

	if (call && call_max > 0)
		call[0] = 0;
	if (pipeline_typed_answer(question, answer, answer_max) == PIPELINE_OK)
		return 0;

	/*
	 * A typed call: exact and instant, and the names it accepts are the ones
	 * the answer pane already prints on its CALL line. A relation whose
	 * arguments do not fit is refused rather than handed to the decoder,
	 * which would guess at a sentence that was never one.
	 */
	/* A plan the student typed: calls separated by ';', each able to use the
	 * number the step before it produced. */
	if (plan_is(question)) {
		plan_t plan;

		if (plan_execute(question, &plan) < 0) {
			if (call && call_max > 0)
				snprintf(call, (size_t)call_max, "%s", question);
			return 2;
		}
		if (call && call_max > 0)
			snprintf(call, (size_t)call_max, "%s", question);
		snprintf(answer, (size_t)answer_max, "%s",
		         plan.step[plan.n - 1].answer);
		return 0;
	}

	rc = shorthand_build(question, guess, (int)sizeof guess);
	if (rc < 0)
		return 1;
	if (call && call_max > 0)
		snprintf(call, (size_t)call_max, "%s", guess);
	if (rc == 1)
		return 2;
	if (run_call_text(guess, answer, answer_max) < 0)
		return 2;
	return 0;
}

int pipeline_answer(pipeline_t *p, const char *question,
                    char *answer, int answer_max,
                    char *call, int call_max)
{
	uint16_t ids[MAX_PROMPT];
	char prompt[MAX_PROMPT + 16];
	int n, pos = 0, gen = 0;
	int rc_offline;
	size_t qlen = strlen(question);

	/* Every answer starts with nothing weighed, so a closed form that answers
	 * before inference cannot leave the previous question's relations behind
	 * for the screen to draw. */
	p->n_alts = 0;

	/*
	 * Typed arithmetic never reaches the model: it needs no weights, no
	 * vocabulary and no inference, so it answers in microseconds instead of
	 * the seconds a decode costs.
	 */
	/*
	 * Arithmetic and typed calls need no model: they answer in microseconds,
	 * and a shorthand is exact where the decoder can be wrong. Both come
	 * before the weights check so a calculator carrying only the app still
	 * answers them.
	 */
	rc_offline = pipeline_answer_offline(question, answer, answer_max,
	                                     call, call_max);
	if (rc_offline == 0)
		return PIPELINE_OK;
	if (rc_offline == 2)
		return PIPELINE_ERR;

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
		/* The separator is out, so this is the step that names the
		 * relation and the only one whose ranking the screen wants. */
		if (strcmp(sofar, "CALL ") == 0)
			keep_alts(p);
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

int pipeline_alternatives(const pipeline_t *p, pipeline_alt_t *out, int max)
{
	int n = p->n_alts;

	if (out == NULL || max < 1)
		return 0;
	if (n > max)
		n = max;
	for (int i = 0; i < n; i++)
		out[i] = p->alts[i];
	return n;
}
