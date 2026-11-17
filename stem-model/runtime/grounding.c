#include <string.h>

#include "grounding.h"

/* Revised 2026-11-16: reject decimal fragments and chemical subscripts. */

/* The rule itself: see grounding.h for why these two allowances exist. */
static int is_digit(char c)
{
	return c >= '0' && c <= '9';
}

int grounding_number_boundary(const char *text, const char *start, int len)
{
	if (len <= 0)
		return 0;
	if (start > text) {
		char c = start[-1];

		/* An underscore is notation, not a digit: "log_2(8)" and "a_12"
		 * write the same quantity a space would separate. */
		if (is_digit(c) || c == '.' || c == ')' || c == ']' ||
		    (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
			return 0;
		/* A signed exponent is not a standalone quantity. */
		if ((c == '-' || c == '+') && start - text >= 2 &&
		    (start[-2] == 'e' || start[-2] == 'E'))
			return 0;
	}
	return !is_digit(start[len]) &&
	       !((start[len] == 'e' || start[len] == 'E') &&
	         (is_digit(start[len + 1]) || start[len + 1] == '+' ||
	          start[len + 1] == '-')) &&
	       !(start[len] == '.' && is_digit(start[len + 1]));
}

int grounding_arg_in_text(const char *arg, const char *question)
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

		if (!grounding_number_boundary(question, p, (int)n))
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

int grounding_call_grounded(const char *call, const char *question)
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
		if (word >= 2 && !grounding_arg_in_text(start, question))
			return 0;
		word++;
	}
	return 1;
}

/* [+-]? (digits [.digits] | .digits): the same shape the call parser accepts. */
static int number_span(const char *s, int *len)
{
	int i = 0, digits = 0;

	if (s[0] == '-' || s[0] == '+')
		i++;
	while (s[i] >= '0' && s[i] <= '9') {
		i++;
		digits++;
	}
	/* A decimal point only belongs to the number when a digit follows it:
	 * "20." at the end of a sentence is twenty, and keeping the full stop
	 * made it a number of the wrong width, which no argument could match. */
	if (s[i] == '.' && s[i + 1] >= '0' && s[i + 1] <= '9') {
		i++;
		while (s[i] >= '0' && s[i] <= '9') {
			i++;
			digits++;
		}
	}
	if (digits == 0)
		return 0;
	*len = i;
	return 1;
}

/* A digit run must end the token: "5x" is a coefficient, not the number 5, and
 * offering it as an argument value would corrupt the question's meaning. */
static int ends_token(const char *s, int len)
{
	char c = s[len];

	return c == '\0' || c == ' ' || c == ',' || c == '.' || c == ')' ||
	       c == ';' || c == '?' || c == ':' || c == '\n';
}

int grounding_spans(const char *text, grounding_span_t *out, int max)
{
	int n = 0;

	if (!text || !out || max <= 0)
		return 0;
	for (const char *p = text; *p && n < max; p++) {
		int len = 0;

		if (!(p[0] >= '0' && p[0] <= '9') &&
		    !((p[0] == '-' || p[0] == '+') && p[1] >= '0' && p[1] <= '9'))
			continue;
		if (!number_span(p, &len))
			continue;
		if (!grounding_number_boundary(text, p, len))
			continue;
		if (!ends_token(p, len))
			continue;
		if (len >= GROUNDING_SPAN_LEN)
			continue;
		memcpy(out[n].text, p, (size_t)len);
		out[n].text[len] = '\0';
		out[n].start = (int)(p - text);
		out[n].end = out[n].start + len;
		n++;
		p += len - 1;
	}
	return n;
}
