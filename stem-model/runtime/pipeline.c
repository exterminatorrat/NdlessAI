#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expr.h"
#include "callmask.h"
#include "matrix.h"
#include "pipeline.h"
#include "plan.h"
#include "quantity.h"
#include "grounding.h"
#include "repair.h"
#include "shorthand.h"
#include "solvers.h"
#include "steps.h"

/*
 * How close the runner-up relation may come before the decode is treated as a
 * guess.
 *
 * Calibrated, not chosen: the split was swept one threshold at a time on the
 * 2,150-row hand-written eval split and the 2,000-row generated gate, two seeds
 * each, and the trade measured is "answers that were wrong and are now refused"
 * against "answers that were right and are now refused". This value catches 22
 * wrong answers for 7 right ones on the hand-written split (3.1:1, 76% of the
 * refusals avoiding a wrong answer, 19 of the 22 wrong on the relation itself)
 * and 1 wrong for 1 right on the generated gate. The curve is flat either side
 * of it: 250 catches 10 for 4 (2.5:1), 750 catches 29 for 10 (2.9:1), 2,000
 * catches 56 for 22 (2.5:1). Inside that band the choice is a taste call, and
 * this is the lowest threshold swept that reaches the 3:1 the ratio test asks
 * for; past 3,000 the ratio falls and keeps falling.
 *
 * The units are pipeline_alt_t.margin's: the gap between the two best relation
 * logits, in fixed point. A margin of zero is an exact tie, and the largest gap
 * this model produces is about 51,000.
 */
#define RELATION_MARGIN_MIN 500

#define PROMPT_SUFFIX " <sep>"
/*
 * The decode's token budget, measured from the trained call shape rather than
 * guessed: "CALL newton2 2 9.8" is 6 tokens, a two-step chain 17, a three-step
 * chain 30 and a four-step chain 39. At the 20 this used to be, a two-step
 * chain only fitted while its numbers stayed short and every three- and
 * four-step chain was cut off mid-call — the runtime could execute a chain it
 * could never be handed. The window (seq, 192) still ends generation on a long
 * question; this is only the ceiling for a short one.
 */
#define MAX_GEN 64
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
	callmask_next(sofar, allow, p->model.vocab, p->allow_chain);
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
 *
 * A chain is grounded step by step, on the text the decode wrote rather than on
 * the numbers the plan computed: only a literal the model chose can be invented,
 * and a "<n>" argument is the answer of a step that already ran. Grounding the
 * executed plan would fail every reference, which is how a chain whose every
 * number was the question's own came back uncertain.
 */
int pipeline_args_grounded(const char *call, const char *question)
{
	char segment[PLAN_TEXT_MAX];

	if (!plan_is(call))
		return grounding_call_grounded(call, question);
	/* A step is "name args" or "CALL name args", so grounding starts after
	 * whichever the decoder wrote. */
	for (int n = 1; plan_step_text(call, n, segment, (int)sizeof segment); n++) {
		int skip = strncmp(segment, "CALL ", 5) == 0 ? 2 : 1;

		if (!grounding_args_grounded(segment, skip, question))
			return 0;
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
	p->margin_min = RELATION_MARGIN_MIN;
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
	/* Limit, tangent line and average rate are answers about an expression
	 * rather than arithmetic on one, and none of them is a function the
	 * parser knows. */
	if (gexpr_calculus_answer(text, answer, answer_max) == 0)
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
	/*
	 * A number the parser had to clamp is not the number that was typed:
	 * "100000/3" became 32767.99998/3 and printed 10923. The parser stores a
	 * clamped literal as exactly Q16_MAX, which nothing else in a parsed
	 * expression produces, so the tree says whether the question was even
	 * representable before it is evaluated.
	 */
	for (int i = 0; i < e.n; i++)
		if (e.node[i].op == G_NUM && e.node[i].num == Q16_MAX)
			return PIPELINE_NO_VALUE;
	{
		q16_t value;

		/* And the arithmetic itself may have no value: 1/0, sqrt(-4),
		 * 9^999. Saturating those printed 32768 for a question with no
		 * answer. */
		if (gexpr_eval_checked(&e, 0, &value) < 0)
			return PIPELINE_NO_VALUE;
		if (gexpr_format(value, answer, answer_max) < 0)
			return PIPELINE_ERR;
	}
	return PIPELINE_OK;
}

/*
 * Is this text arithmetic and nothing else?
 *
 * The exact engine has a fixed node pool (EXPR_MAX_NODES), so a long enough
 * typed expression fails to parse — measured: a flat sum answers at 64 terms
 * (127 characters) and stops at 65 (129). When that happens the text used to
 * fall through to the decoder, which reads it as a word problem and answers
 * with a relation's worth of invented numbers: the one outcome this program
 * exists to avoid. Arithmetic that is too big for the pool is still arithmetic,
 * so the caller refuses it instead of guessing at it.
 *
 * The test is deliberately narrow: digits, the operators, brackets, and the
 * parser's own function names. A question mark, an equals sign, a variable or
 * any other word means the text is prose and the decoder may read it — "2+2=?"
 * and "what is 2+2?" both still reach the model. Euler's "e" is left out for
 * the same reason: "1e5" is scientific notation, not a product, and the parser
 * reads neither.
 */
static int looks_like_arithmetic(const char *text)
{
	static const char *const funcs[] = {
		"sqrt", "abs", "ln", "exp", "sin", "cos", "tan"
	};
	int digits = 0;

	if (!text)
		return 0;
	for (const char *p = text; *p;) {
		int matched = 0;

		if (*p >= '0' && *p <= '9') {
			digits++;
			p++;
			continue;
		}
		if (*p == '.' || *p == '+' || *p == '-' || *p == '*' || *p == '/' ||
		    *p == '^' || *p == '(' || *p == ')' || *p == ',' || *p == ' ') {
			p++;
			continue;
		}
		for (int i = 0; i < (int)(sizeof funcs / sizeof funcs[0]); i++) {
			size_t n = strlen(funcs[i]);

			if (strncmp(p, funcs[i], n) == 0) {
				p += n;
				matched = 1;
				break;
			}
		}
		if (!matched)
			return 0;
	}
	return digits > 0;
}

int pipeline_answer_offline(const char *question, char *answer, int answer_max,
                            char *call, int call_max)
{
	char guess[64];
	int rc;

	if (call && call_max > 0)
		call[0] = 0;
	rc = pipeline_typed_answer(question, answer, answer_max);
	if (rc == PIPELINE_OK)
		return PIPELINE_OFFLINE_ANSWERED;
	if (rc == PIPELINE_NO_VALUE)
		return PIPELINE_OFFLINE_NO_VALUE;
	/* Arithmetic the exact engine could not hold is not a word problem, and
	 * the decoder would read it as one. Refuse it here, with a code of its own
	 * so the screen can say which of the three happened. */
	if (looks_like_arithmetic(question))
		return PIPELINE_OFFLINE_TOO_LONG;

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
			return PIPELINE_OFFLINE_REFUSED;
		}
		if (call && call_max > 0)
			snprintf(call, (size_t)call_max, "%s", question);
		snprintf(answer, (size_t)answer_max, "%s",
		         plan.step[plan.n - 1].answer);
		return 0;
	}

	rc = shorthand_build(question, guess, (int)sizeof guess);
	if (rc < 0)
		return PIPELINE_OFFLINE_NOT_MINE;
	if (call && call_max > 0)
		snprintf(call, (size_t)call_max, "%s", guess);
	if (rc == 1)
		return PIPELINE_OFFLINE_REFUSED;
	if (run_call_text(guess, answer, answer_max) < 0)
		return PIPELINE_OFFLINE_REFUSED;
	return PIPELINE_OFFLINE_ANSWERED;
}

int pipeline_answer(pipeline_t *p, const char *question,
                    char *answer, int answer_max,
                    char *call, int call_max)
{
	uint16_t ids[MAX_PROMPT];
	char prompt[MAX_PROMPT + 16];
	char checked[256];
	int n, pos = 0, gen = 0;
	int rc_offline, grounded, units;
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
	if (rc_offline == PIPELINE_OFFLINE_ANSWERED)
		return PIPELINE_OK;
	if (rc_offline != PIPELINE_OFFLINE_NOT_MINE) {
		/* A refusal the offline path understood: carry its reason through. */
		if (rc_offline == PIPELINE_OFFLINE_TOO_LONG)
			return PIPELINE_TOO_LONG;
		if (rc_offline == PIPELINE_OFFLINE_NO_VALUE)
			return PIPELINE_NO_VALUE;
		return PIPELINE_ERR;
	}

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
	/*
	 * A relation the decode all but tied is a guess, and a guess printed as an
	 * answer is the failure this program is built around. The margin is
	 * already collected for the screen, so asking it here costs nothing and
	 * refuses before the solver runs: the answer is empty, the call is still
	 * there for the screen to show what it nearly said, and the alternatives
	 * are still ranked behind it.
	 */
	if (p->margin_min > 0 && p->n_alts > 1 &&
	    p->alts[1].margin < p->margin_min) {
		answer[0] = '\0';
		return PIPELINE_UNCERTAIN;
	}
	/*
	 * The dominant failure on hand-written wording is a digit one keystroke
	 * off, not the wrong relation. Snap an ungrounded argument back to the one
	 * number in the sentence it can only have meant, before the grounding
	 * check decides the answer is uncertain.
	 */
	if (call && call_max > 0) {
		char repaired[256];

		if (repair_call(call, question, repaired, (int)sizeof repaired) > 0 &&
		    strlen(repaired) < (size_t)call_max)
			snprintf(call, (size_t)call_max, "%s", repaired);
	}
	/*
	 * Grounding is decided on the numbers as decoded, before any of them is
	 * rewritten in another unit, because the rewritten ones are by design not
	 * in the text. Then the units: a number written in a unit its slot cannot
	 * take is an argument in the wrong place, and a convertible one is
	 * rewritten so the solver works in the unit its formula assumes.
	 */
	grounded = pipeline_args_grounded(call, question);
	units = quantity_check_call(call, question, checked, (int)sizeof checked);
	if (units == QUANTITY_MISMATCH) {
		answer[0] = '\0';
		return PIPELINE_UNITS;
	}
	if (units == QUANTITY_CONVERTED) {
		if (strlen(checked) >= (size_t)call_max)
			return PIPELINE_ERR;
		memcpy(call, checked, strlen(checked) + 1);
	}
	if (run_call_text(call, answer, answer_max) < 0) {
		/*
		 * A solver refusal after a conversion is about the converted numbers:
		 * the question was read, its numbers were in the text, and the units
		 * were rewritten exactly. Only when grounding held, so an invented
		 * argument is still an unreadable call rather than a number too big.
		 */
		if (units == QUANTITY_CONVERTED && grounded)
			return PIPELINE_UNITS_LIMIT;
		return PIPELINE_ERR;
	}
	if (!grounded) {
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
