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
#include "solvers.h"
#include "steps.h"

#define PLAN_TEXT_MAX 256

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

int plan_is(const char *text)
{
	return text && strchr(text, ';') != NULL;
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
	int n = 0;

	for (char *at = work; *at && n < 10;) {
		while (*at == ' ')
			*at++ = '\0';
		if (!*at)
			break;
		parts[n++] = at;
		while (*at && *at != ' ')
			at++;
	}
	/*
	 * Both spellings are a step: "CALL name a b" is what the decoder emits and
	 * what the answer pane prints, and "name a b" is what a student types,
	 * matching the shorthand the screen already accepts. A step with no name
	 * is the only thing refused here; whether the name resolves is the
	 * solver's answer to give.
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

int plan_render(const plan_t *p, char *out, int outlen)
{
	int used = 0, wrote = 0;

	if (!out || outlen < 1)
		return -1;
	out[0] = '\0';
	for (int i = 0; i < p->n; i++) {
		const plan_step_t *step = &p->step[i];
		char block[192];
		const char *argv[8];
		int rc;

		argv_of(step, argv);
		rc = steps_render(step->name, argv, step->nargs, step->answer, block,
		                  (int)sizeof block);

		if (rc <= 0)
			continue;
		/* "1." then the block's own first line, so the step number reads as
		 * the plan's numbering and not as part of the formula. */
		if (used < outlen - 1)
			used += snprintf(out + used, (size_t)(outlen - used), "%d. ",
			                 i + 1);
		for (const char *s = block; *s && used < outlen - 1; s++) {
			out[used++] = *s;
			if (*s == '\n' && used < outlen - 2) {
				out[used++] = ' ';
				out[used++] = ' ';
			}
		}
		if (used < outlen - 1 && out[used - 1] != '\n')
			out[used++] = '\n';
		wrote = 1;
	}
	if (used < outlen)
		out[used] = '\0';
	return wrote ? (used < outlen ? 1 : -1) : 0;
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
