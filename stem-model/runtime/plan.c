/*
 * Multi-step plans: a chain of calls, each able to use the number the step
 * before produced.
 *
 * This file is the grammar and the plumbing. It does not decide *which* chain
 * a question needs — that is the model's job once it can emit one, and typing
 * the plan by hand is what makes the runtime testable before then.
 */
#include <stdio.h>
#include <string.h>

#include "plan.h"
#include "reltable.h"
#include "solvers.h"
#include "steps.h"

/* [+-]? (digits [.digits] | .digits) [e [+-]? digits] */
static int is_number(const char *s)
{
	int digits = 0;

	if (*s == '+' || *s == '-')
		s++;
	while (*s >= '0' && *s <= '9') {
		s++;
		digits++;
	}
	if (*s == '.') {
		s++;
		while (*s >= '0' && *s <= '9') {
			s++;
			digits++;
		}
	}
	if (digits == 0)
		return 0;
	if (*s == 'e' || *s == 'E') {
		s++;
		if (*s == '+' || *s == '-')
			s++;
		if (!(*s >= '0' && *s <= '9'))
			return 0;
		while (*s >= '0' && *s <= '9')
			s++;
	}
	return *s == '\0';
}

/* "<n>" and nothing else: the whole token has to be the reference. */
static int ref_index(const char *s)
{
	int n = 0, digits = 0;

	if (*s++ != '<')
		return -1;
	while (*s >= '0' && *s <= '9') {
		n = n * 10 + (*s - '0');
		s++;
		digits++;
	}
	if (*s != '>' || *(s + 1) != '\0' || digits == 0)
		return -1;
	return n;
}

/*
 * The number at the front of an answer, copied out for the next step. A solver
 * answer is a value and an optional unit ("200 N", "-3.4 eV", "1.871e+07 Bq"),
 * so the magnitude is the leading numeric run. An answer with an '=' or a ','
 * before the unit is several values ("x=0, x=3"), and those cannot be chained.
 */
static int magnitude_of(const char *answer, char *out, int outlen)
{
	int n = 0;

	if (strchr(answer, '=') != NULL || strchr(answer, ',') != NULL)
		return -1;
	while (answer[n] && n < outlen - 1 && n < 23) {
		char c = answer[n];

		if ((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+' ||
		    c == 'e' || c == 'E') {
			/* 'e' only belongs to the number as an exponent marker, and the
			 * unit "eV" starts with one: accept it only after a digit. */
			if ((c == 'e' || c == 'E') &&
			    (n == 0 || !(answer[n - 1] >= '0' && answer[n - 1] <= '9')))
				break;
			out[n++] = c;
		} else {
			break;
		}
	}
	out[n] = '\0';
	return n > 0 && is_number(out) ? 0 : -1;
}

/* Splits `work` on spaces into words, in place. Returns the count, or -1 when
 * there are more words than a call can carry. */
static int words_of(char *work, char **parts, int max)
{
	int n = 0;

	for (char *at = work; *at;) {
		while (*at == ' ')
			*at++ = '\0';
		if (!*at)
			break;
		if (n == max)
			return -1;
		parts[n++] = at;
		while (*at && *at != ' ')
			at++;
	}
	return n;
}

/* The registry row a step names exactly, or NULL. Exactly, not as a shorthand:
 * solvers_solve() dispatches on the name the answer pane prints, so a prefix
 * shorthand_build() would accept here could never run as a step. */
static const rel_row_t *relation_named(const char *name)
{
	for (int i = 0; i < REL_NUM_RELATIONS; i++)
		if (strcmp(name, rel_rows[i].name) == 0)
			return &rel_rows[i];
	return NULL;
}

/*
 * Whether one ';'-separated segment is a step the solvers could run: an
 * optional CALL, the relation's own name, then arguments that are numbers or
 * <n> references.
 *
 * The argument count is deliberately not checked. A step with the wrong count
 * is a plan that failed, and plan_execute names the step that failed — which is
 * worth more to a student than being sent back to the decoder.
 */
static int segment_is_step(const char *segment)
{
	char work[PLAN_TEXT_MAX];
	char *parts[10];
	int n, first;

	if (strlen(segment) >= sizeof work)
		return 0;
	strcpy(work, segment);
	n = words_of(work, parts, 10);
	if (n < 1)
		return 0;
	first = strcmp(parts[0], "CALL") == 0 ? 1 : 0;
	if (n - first < 1 || relation_named(parts[first]) == NULL)
		return 0;
	for (int i = first + 1; i < n; i++)
		if (!is_number(parts[i]) && ref_index(parts[i]) < 0)
			return 0;
	return 1;
}

/*
 * Copies the step at `start` up to the next ';', trimmed, into out. Returns a
 * pointer past the separator, or NULL at the end of the text.
 */
static const char *next_segment(const char *start, char *out, int outlen)
{
	const char *end = strchr(start, ';');
	int len;

	if (!end)
		end = start + strlen(start);
	len = (int)(end - start);
	while (len > 0 && *start == ' ') {
		start++;
		len--;
	}
	while (len > 0 && start[len - 1] == ' ')
		len--;
	if (len >= outlen)
		return NULL;
	memcpy(out, start, (size_t)len);
	out[len] = '\0';
	return *end ? end + 1 : NULL;
}

/*
 * Whether `text` is a plan rather than a question that happens to contain a
 * separator.
 *
 * Prose is separated by ';' as often as a chain is — "the bottle is 0.5 M; 25 mL
 * is measured out" — and the two mistakes are not the same size. A question
 * read as a plan loses the answer completely, because plan_execute fails and the
 * caller never decodes it, while a chain read as a question only reaches the
 * decoder, which refuses what it cannot parse. So the benefit of the doubt goes
 * to the question: every segment has to be a call the solvers could run.
 */
int plan_is(const char *text)
{
	char segment[PLAN_TEXT_MAX];
	const char *rest;

	if (!text || strchr(text, ';') == NULL)
		return 0;
	rest = text;
	while (rest) {
		rest = next_segment(rest, segment, (int)sizeof segment);
		if (segment[0] && !segment_is_step(segment))
			return 0;
	}
	return 1;
}

/* The solvers take an array of pointers; a step stores its arguments inline. */
static void argv_of(const plan_step_t *step, const char *argv[8])
{
	for (int i = 0; i < step->nargs; i++)
		argv[i] = step->args[i];
}

/* Splits "CALL name a b" into the step's own buffers. */
static int split_call(char *work, plan_step_t *step)
{
	char *parts[10];
	int n = words_of(work, parts, 10);

	if (n < 1)
		return -1;
	/*
	 * Both spellings are a step: "CALL name a b" is what the decoder emits and
	 * what the answer pane prints, and "name a b" is what a student types,
	 * matching the shorthand the screen already accepts. A step with no name
	 * is the only thing refused here: plan_is has already refused a segment
	 * that names no relation, so what is left for the solver to answer is
	 * whether the numbers work.
	 */
	{
		int first = strcmp(parts[0], "CALL") == 0 ? 1 : 0;

		if (n - first < 1 || n - first - 1 > 8)
			return -1;
		if (strlen(parts[first]) >= sizeof step->name)
			return -1;
		strcpy(step->name, parts[first]);
		step->nargs = n - first - 1;
		for (int i = 0; i < step->nargs; i++) {
			if (strlen(parts[first + 1 + i]) >= sizeof step->args[0])
				return -1;
			strcpy(step->args[i], parts[first + 1 + i]);
		}
	}
	return 0;
}

int plan_execute(const char *text, plan_t *p)
{
	char segment[PLAN_TEXT_MAX];
	const char *rest;

	if (!p)
		return -1;
	memset(p, 0, sizeof *p);
	if (!text || !plan_is(text))
		return -1;
	if (strlen(text) >= PLAN_TEXT_MAX) {
		snprintf(p->err, sizeof p->err, "the plan is too long");
		return -2;
	}

	rest = text;
	while (rest) {
		plan_step_t *step;
		char buffer[PLAN_TEXT_MAX];
		int more;

		more = 0;
		rest = next_segment(rest, segment, (int)sizeof segment);
		if (!segment[0]) {
			if (rest)
				continue;
			break;
		}
		if (p->n == PLAN_MAX_STEPS) {
			snprintf(p->err, sizeof p->err, "a plan is at most %d steps",
			         PLAN_MAX_STEPS);
			return -2;
		}
		step = &p->step[p->n];
		strcpy(buffer, segment);
		if (split_call(buffer, step) < 0) {
			snprintf(p->err, sizeof p->err, "step %d is not a call",
			         p->n + 1);
			return -2;
		}
		p->n++;
		more = rest != NULL;
		for (int i = 0; i < step->nargs; i++) {
			int ref = ref_index(step->args[i]);

			if (ref < 0)
				continue;
			if (ref < 1 || ref >= p->n) {
				snprintf(p->err, sizeof p->err,
				         "step %d uses step %d, which has not run yet",
				         p->n, ref);
				return -2;
			}
			strcpy(step->args[i], p->step[ref - 1].magnitude);
		}
		{
			const char *argv[8];

			argv_of(step, argv);
			if (solvers_solve(step->name, argv, step->nargs, step->answer,
			                  sizeof step->answer) < 0) {
				snprintf(p->err, sizeof p->err,
				         "step %d: %s did not answer those numbers", p->n,
				         step->name);
				return -2;
			}
		}
		if (magnitude_of(step->answer, step->magnitude,
		                 (int)sizeof step->magnitude) < 0) {
			snprintf(p->err, sizeof p->err,
			         "step %d answers with more than one number", p->n);
			return -2;
		}
		if (!more)
			break;
	}
	if (p->n == 0) {
		snprintf(p->err, sizeof p->err, "there are no steps");
		return -2;
	}
	return 0;
}

/* Appends s, or reports that the buffer is full. Always keeps room for the NUL. */
static int plan_put(char **at, char *end, const char *s)
{
	while (*s) {
		if (*at >= end)
			return -1;
		*(*at)++ = *s++;
	}
	return 0;
}

static int plan_put_ch(char **at, char *end, char c)
{
	if (*at >= end)
		return -1;
	*(*at)++ = c;
	return 0;
}

int plan_render(const plan_t *p, char *out, int outlen)
{
	char *at = out;
	char *end;
	int wrote = 0, clipped = 0;

	if (!out || outlen < 1)
		return -1;
	out[0] = '\0';
	end = out + outlen - 1;
	for (int i = 0; i < p->n && !clipped; i++) {
		const plan_step_t *step = &p->step[i];
		char block[192];
		char head[16];
		const char *argv[8];
		int rc;

		argv_of(step, argv);
		rc = steps_render(step->name, argv, step->nargs, step->answer, block,
		                  (int)sizeof block);

		/* The separator belongs to the step after the first, so the plan ends
		 * on its last answer rather than on a blank line: the answer pane
		 * brightens the final line and dims everything above it. */
		if (i > 0 && plan_put(&at, end, "\n") < 0)
			clipped = 1;
		snprintf(head, sizeof head, "%d. ", i + 1);
		if (plan_put(&at, end, head) < 0)
			clipped = 1;
		if (rc > 0) {
			/* "1." then the block's own first line, so the step number reads
			 * as the plan's numbering and not as part of the formula. Each
			 * continuation line is indented under it. */
			for (const char *s = block; *s; s++) {
				if (plan_put_ch(&at, end, *s) < 0 ||
				    (*s == '\n' && plan_put(&at, end, "  ") < 0)) {
					clipped = 1;
					break;
				}
			}
		} else {
			/* A relation with no step template still has to appear. An
			 * intermediate the working skips is one the student has to invent,
			 * and the call's own numbers are the only working such a relation
			 * has. */
			if (plan_put(&at, end, step->name) < 0)
				clipped = 1;
			for (int a = 0; a < step->nargs; a++) {
				if (plan_put(&at, end, " ") < 0 ||
				    plan_put(&at, end, step->args[a]) < 0)
					clipped = 1;
			}
			if (plan_put(&at, end, "\n  = ") < 0 ||
			    plan_put(&at, end, step->answer) < 0)
				clipped = 1;
		}
		wrote = 1;
	}
	*at = '\0';
	return wrote ? (clipped ? -1 : 1) : 0;
}

const char *plan_step_text(const char *text, int n, char *out, int outlen)
{
	char segment[PLAN_TEXT_MAX];
	const char *rest = text;
	int seen = 0;

	while (rest && *rest) {
		rest = next_segment(rest, segment, (int)sizeof segment);
		if (++seen == n) {
			snprintf(out, (size_t)outlen, "%s", segment);
			return out;
		}
	}
	return NULL;
}
