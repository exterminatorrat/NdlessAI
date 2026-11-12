/*
 * Typed shorthands: "ff 0.4 500" answers with no decoder in the path.
 *
 * Two properties decide whether this is useful rather than surprising:
 *
 *  - It must never swallow a sentence. A shorthand is a name token followed
 *    only by numbers, so "a 5 kg crate slides..." and "integrate(x^2)" cannot
 *    match however they are written.
 *  - It must never answer the wrong relation. The full name always matches;
 *    anything shorter has to be unique among the 136 names, and a two-letter
 *    mnemonic is refused when two relations would share it.
 */
#include <stdio.h>
#include <string.h>

#include "reltable.h"
#include "shorthand.h"

#define TOKENS    9
#define TOKLEN    24
#define MIN_PREFIX 3

static int lower(int c)
{
	return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

/*
 * A name token: letters, digits and underscores only. Underscores are what
 * the answer pane prints, and a hyphen is accepted for the same name because
 * it is the easiest character to reach on a keypad.
 */
static int is_name_char(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
	       (c >= '0' && c <= '9') || c == '_' || c == '-';
}

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

static int name_eq(const char *tok, const char *name)
{
	size_t i = 0;

	for (; tok[i] && name[i]; i++)
		if (lower(tok[i]) != lower(name[i]) &&
		    !(tok[i] == '-' && name[i] == '_'))
			return 0;
	return tok[i] == '\0' && name[i] == '\0';
}

static int name_prefix(const char *tok, const char *name)
{
	size_t i = 0;

	for (; tok[i] && name[i]; i++)
		if (lower(tok[i]) != lower(name[i]) &&
		    !(tok[i] == '-' && name[i] == '_'))
			return 0;
	return tok[i] == '\0';
}

/*
 * "ff" for friction_force: one letter per underscore-separated part. Only the
 * parts a name actually has, so a one-word name has a one-letter mnemonic and
 * is always longer than the shortest prefix anyone would type.
 */
static void initials_of(const char *name, char *out, int outlen)
{
	int n = 0;

	out[n++] = name[0];
	for (const char *p = name; *p && n < outlen - 1; p++)
		if (p[0] == '_' && p[1])
			out[n++] = p[1];
	out[n] = '\0';
}

static int same_initials(const char *a, const char *b)
{
	char ib[TOKLEN];

	initials_of(b, ib, (int)sizeof ib);
	return strcmp(a, ib) == 0;
}

/*
 * The relation a name token means, or NULL. Ambiguity is refused rather than
 * resolved by table order: a shorthand that silently means one of two
 * relations is worse than no shorthand.
 */
static const rel_row_t *resolve(const char *tok)
{
	const rel_row_t *prefix = NULL;
	int prefix_n = 0;

	if (!tok[0] || !is_name_char(tok[0]) ||
	    (tok[0] >= '0' && tok[0] <= '9'))
		return NULL;

	for (int i = 0; i < REL_NUM_RELATIONS; i++) {
		const rel_row_t *row = &rel_rows[i];

		if (name_eq(tok, row->name))
			return row;
		if (strcmp(tok, row->name) != 0) {
			char initials[TOKLEN];
			int matches = 0;

			initials_of(row->name, initials, (int)sizeof initials);
			if (name_eq(tok, initials)) {
				for (int j = 0; j < REL_NUM_RELATIONS; j++)
					if (same_initials(tok, rel_rows[j].name))
						matches++;
				if (matches == 1)
					return row;
			}
		}
		if (name_prefix(tok, row->name)) {
			prefix = row;
			prefix_n++;
		}
	}

	if (prefix_n == 1 && strlen(tok) >= MIN_PREFIX)
		return prefix;
	return NULL;
}

/*
 * Splits on spaces. Returns the token count, or -1 when the text cannot be a
 * shorthand: too many tokens, a token too long, or a letter after the first
 * token, which is what keeps sentences out.
 */
static int split(const char *text, char tok[TOKENS][TOKLEN])
{
	int n = 0;
	size_t i = 0;

	while (text[i]) {
		size_t len = 0;

		while (text[i] == ' ')
			i++;
		if (!text[i])
			break;
		if (n == TOKENS)
			return -1;
		while (text[i] && text[i] != ' ') {
			if (len == TOKLEN - 1)
				return -1;
			tok[n][len++] = text[i++];
		}
		tok[n][len] = '\0';
		n++;
	}
	return n;
}

static void build(const rel_row_t *row, char tok[TOKENS][TOKLEN], int n,
                  char *call, int call_max)
{
	int used = snprintf(call, (size_t)call_max, "CALL %s", row->name);

	for (int i = 1; i < n && used > 0 && used < call_max; i++)
		used += snprintf(call + used, (size_t)(call_max - used), " %s",
		                 tok[i]);
}

int shorthand_build(const char *text, char *call, int call_max)
{
	char tok[TOKENS][TOKLEN];
	const rel_row_t *row;
	int n;

	if (!text || !call || call_max < 1)
		return -1;
	call[0] = '\0';
	n = split(text, tok);
	if (n < 2)
		return -1;
	for (int i = 1; i < n; i++)
		if (!is_number(tok[i]))
			return -1;
	row = resolve(tok[0]);
	if (!row)
		return -1;
	build(row, tok, n, call, call_max);
	return n - 1 == (int)row->arity ? 0 : 1;
}
