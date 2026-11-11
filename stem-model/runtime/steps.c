#include <string.h>

#include "steps.h"

#define STEPTABLE_IMPL
#include "steptable.h"

/* Appends s, or fails once the buffer is full. Always keeps room for the NUL. */
static int put(char **at, char *end, const char *s)
{
	while (*s && *at < end)
		*(*at)++ = *s++;
	return *s ? -1 : 0;
}

static int put_char(char **at, char *end, char c)
{
	if (*at >= end)
		return -1;
	*(*at)++ = c;
	return 0;
}

static const step_row_t *find_row(const char *name)
{
	int i;

	if (!name)
		return NULL;
	for (i = 0; i < STEP_NUM_ROWS; i++)
		if (strcmp(step_rows[i].name, name) == 0)
			return &step_rows[i];
	return NULL;
}

/*
 * Copies expr, replacing "@1".."@n" with the argument they name. An argument
 * number outside the call's arity is a template bug rather than a user error,
 * so it renders as "?" instead of reading past the array.
 */
static int put_expr(char **at, char *end, const char *expr,
                    const char *const *args, int nargs)
{
	const char *p = expr;

	while (*p) {
		if (*p == '@') {
			int idx = 0;
			int have = 0;

			p++;
			while (*p >= '0' && *p <= '9') {
				idx = idx * 10 + (*p - '0');
				have = 1;
				p++;
			}
			if (!have) {
				if (put_char(at, end, '@') < 0)
					return -1;
				continue;
			}
			if (idx < 1 || idx > nargs || !args[idx - 1]) {
				if (put_char(at, end, '?') < 0)
					return -1;
				continue;
			}
			if (put(at, end, args[idx - 1]) < 0)
				return -1;
			continue;
		}
		if (put_char(at, end, *p++) < 0)
			return -1;
	}
	return 0;
}

int steps_render(const char *name, const char *const *args, int nargs,
                 const char *answer, char *out, int outlen)
{
	const step_row_t *row;
	char *at = out;
	char *end;
	int rc = 0;

	if (!out || outlen < 2)
		return -1;
	end = out + outlen - 1;

	row = find_row(name);
	if (!row || !row->expr || !row->expr[0]) {
		out[0] = 0;
		return 0;
	}

	if (row->line && row->line[0]) {
		if (put(&at, end, row->line) < 0)
			rc = -1;
		if (put(&at, end, "\n= ") < 0)
			rc = -1;
	} else if (put(&at, end, "= ") < 0) {
		rc = -1;
	}

	if (put_expr(&at, end, row->expr, args, nargs) < 0)
		rc = -1;
	if (answer && answer[0]) {
		if (put(&at, end, "\n= ") < 0)
			rc = -1;
		if (put(&at, end, answer) < 0)
			rc = -1;
	}

	*at = 0;
	return rc == 0 ? 1 : -1;
}
