/*
 * Digit repair: the right relation with a number one keystroke off.
 *
 * The rules live in repair.h. What matters here is that this file never invents
 * a number: it only picks, from the numbers already in the sentence, the one a
 * mis-decoded argument can only have meant. Everything else is left alone so
 * the caller's grounding check can refuse it.
 */
#include <stdio.h>
#include <string.h>

#include "grounding.h"
#include "repair.h"

#define MAX_NUMBERS 24
#define NUMBER_LEN 24

typedef struct {
	char text[NUMBER_LEN];
	int start;
	int end;
} number_t;

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
 * repairing into it would corrupt the question's meaning. */
static int ends_token(const char *s, int len)
{
	char c = s[len];

	return c == '\0' || c == ' ' || c == ',' || c == '.' || c == ')' ||
	       c == ';' || c == '?' || c == ':' || c == '\n';
}

static int collect(const char *text, number_t *out, int max)
{
	int n = 0;

	for (const char *p = text; *p && n < max; p++) {
		int len = 0;

		if (!(p[0] >= '0' && p[0] <= '9') &&
		    !((p[0] == '-' || p[0] == '+') && p[1] >= '0' && p[1] <= '9'))
			continue;
		if (!number_span(p, &len))
			continue;
		if (!ends_token(p, len))
			continue;
		if (len >= NUMBER_LEN)
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

/* Numbers are compared on their digits alone: "-0.1" and "0.1" differ by a
 * sign, which is the polarity question, not the digit question. */
static const char *digits_of(const char *s)
{
	return (*s == '-' || *s == '+') ? s + 1 : s;
}

static int digit_length(const char *s)
{
	return (int)strlen(digits_of(s));
}

/* One substitution and nothing else: same sign, same length, one character
 * different. The sign is not a digit — repairing 6 into -3 would be changing
 * the physics, not fixing a keystroke. */
static int one_off(const char *a, const char *b)
{
	int diffs = 0;
	int sign_a = a[0] == '-' ? -1 : 1;
	int sign_b = b[0] == '-' ? -1 : 1;

	if (sign_a != sign_b)
		return 0;
	a = digits_of(a);
	b = digits_of(b);
	if (digit_length(a) != digit_length(b))
		return 0;
	for (int i = 0; a[i]; i++)
		if (a[i] != b[i])
			diffs++;
	return diffs == 1;
}

int repair_argument(const char *arg, const char *question, char *out, int outlen)
{
	number_t numbers[MAX_NUMBERS];
	int n, best = -1, ties = 0;

	if (!arg || !question || !out || outlen < 2)
		return -1;
	out[0] = '\0';
	n = collect(question, numbers, MAX_NUMBERS);
	for (int i = 0; i < n; i++) {
		if (!one_off(arg, numbers[i].text))
			continue;
		/* Two candidates as close as each other means the difference that
		 * makes one right is not in the digits: refuse. */
		if (best >= 0 && strcmp(numbers[best].text, numbers[i].text) != 0)
			ties = 1;
		if (best < 0)
			best = i;
	}
	if (best < 0 || ties)
		return 0;
	snprintf(out, (size_t)outlen, "%s", numbers[best].text);
	return 1;
}

/* Is `token` one of the other numbers already in the call? */
static int duplicated(char *const tokens[], int n, int skip, const char *candidate)
{
	for (int i = 0; i < n; i++)
		if (i != skip && !strcmp(digits_of(tokens[i]), digits_of(candidate)))
			return 1;
	return 0;
}

/* A token the call parser would read as a number, rather than CALL, the
 * relation's name, a step reference or a separator. */
static int numeric(const char *token)
{
	if (token[0] == '<' || token[0] == ';')
		return 0;
	if (token[0] >= '0' && token[0] <= '9')
		return 1;
	return (token[0] == '-' || token[0] == '+') &&
	       token[1] >= '0' && token[1] <= '9';
}

/*
 * One argument, at most. Two ungrounded numbers in the same call are not a
 * keystroke, they are a decode that went wrong, and snapping both would stack
 * two guesses into one confident answer. The replacement also may not repeat a
 * number the call already has: "pythagoras 2 7" is one character from "7 7"
 * and 7 7 is not what anyone meant.
 */
int repair_call(const char *call, const char *question, char *out, int outlen)
{
	char work[256];
	char *tokens[16];
	char candidate[NUMBER_LEN];
	int n = 0, hit = -1, used = 0;

	if (!call || !question || !out || outlen < 2)
		return -1;
	if (strlen(call) >= sizeof work || (size_t)outlen < strlen(call) + 1)
		return -1;
	strcpy(work, call);

	/* Split on spaces by hand rather than with strtok_r: this file is on the
	 * device path, where POSIX is not declared and an implicit strtok_r is a
	 * pointer built from a returned int. */
	for (char *p = work; *p && n < 16; p++) {
		if (*p == ' ') {
			*p = '\0';
			continue;
		}
		if (p == work || p[-1] == '\0')
			tokens[n++] = p;
	}

	for (int i = 0; i < n; i++) {
		char fixed[NUMBER_LEN];

		if (!numeric(tokens[i]) ||
		    grounding_arg_in_text(tokens[i], question))
			continue;
		if (repair_argument(tokens[i], question, fixed,
		                    (int)sizeof fixed) != 1)
			continue;
		if (hit >= 0) {
			hit = -1; /* two repairs: trust neither */
			break;
		}
		hit = i;
		snprintf(candidate, sizeof candidate, "%s", fixed);
	}
	if (hit >= 0 && duplicated(tokens, n, hit, candidate))
		hit = -1;

	for (int i = 0; i < n; i++) {
		const char *word = i == hit ? candidate : tokens[i];

		used += snprintf(out + used, (size_t)outlen - (size_t)used,
		                 "%s%s", used ? " " : "", word);
	}
	return hit >= 0;
}
