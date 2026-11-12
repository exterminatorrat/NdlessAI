#include <string.h>

#include "grounding.h"

/* The rule itself: see grounding.h for why these two allowances exist. */
static int is_digit(char c)
{
	return c >= '0' && c <= '9';
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
