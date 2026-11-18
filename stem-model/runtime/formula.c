/*
 * Formulas the app computes for itself: a composition, a molar mass, and how
 * many ions one formula unit supplies.
 *
 * Both halves of this file are exact. The weights are integer thousandths of
 * a gram per mole, so a molar mass is a rational that dec4_render prints the
 * way every other solver prints an answer; there is no float to disagree with
 * the Python reference. The parser walks the text once and refuses on the
 * first thing it cannot read.
 */
#include <stdio.h>
#include <string.h>

#define CHEMTABLE_IMPL
#include "chemtable.h"

#include "formula.h"
#include "solvers_subject.h"

#define MAX_DEPTH 8
#define MAX_ATOMS 1000
#define MAX_COUNT 999
#define MAX_TERMS 8

typedef struct {
	unsigned short count[CHEM_NUM_ELEMENTS];
	int total;
} comp_t;

static int is_digit(char c)
{
	return c >= '0' && c <= '9';
}

static int is_upper(char c)
{
	return c >= 'A' && c <= 'Z';
}

static int is_lower(char c)
{
	return c >= 'a' && c <= 'z';
}

/*
 * ASCII digits and a dot for every hydrate separator. The unicode subscripts a
 * bank writes are three bytes each and map to their digit; any other non-ASCII
 * byte is refused rather than passed through as a symbol.
 */
static int normalise(const char *in, char *out, int outlen)
{
	const unsigned char *p = (const unsigned char *)in;
	int n = 0;

	if (in == NULL)
		return -3;
	while (*p) {
		char c;

		if (p[0] == 0xE2 && p[1] == 0x82 && p[2] >= 0x80 && p[2] <= 0x89) {
			c = (char)('0' + (p[2] - 0x80));
			p += 3;
		} else if (p[0] == 0xC2 && p[1] == 0xB7) {
			c = '.';
			p += 2;
		} else if (p[0] == 0xE2 && p[1] == 0x80 && p[2] == 0xA2) {
			c = '.';
			p += 3;
		} else if (*p == '*') {
			c = '.';
			p++;
		} else if (*p >= 0x80) {
			return -1;
		} else {
			c = (char)*p;
			p++;
		}
		if (n >= outlen - 1)
			return -4;
		out[n++] = c;
	}
	out[n] = '\0';
	return n > 0 ? 0 : -3;
}

static int element_index(const char *sym, size_t len)
{
	int i;

	for (i = 0; i < CHEM_NUM_ELEMENTS; i++)
		if (strlen(chem_elements[i].symbol) == len &&
		    memcmp(chem_elements[i].symbol, sym, len) == 0)
			return i;
	return -1;
}

/* One subscript. No digits means one atom, and *pos is left alone. */
static int read_count(const char *s, int *pos)
{
	int i = *pos, n = 0, digits = 0;

	while (is_digit(s[i])) {
		n = n * 10 + (s[i] - '0');
		i++;
		digits++;
		if (n > MAX_COUNT)
			return -1;
	}
	if (!digits)
		return 1;
	if (n < 1)
		return -1;
	*pos = i;
	return n;
}

static int merge_term(comp_t *into, const comp_t *sub, int multiplier)
{
	int i;

	for (i = 0; i < CHEM_NUM_ELEMENTS; i++) {
		if (!sub->count[i])
			continue;
		into->count[i] = (unsigned short)(into->count[i] +
		                                  sub->count[i] * multiplier);
	}
	into->total += sub->total * multiplier;
	return into->total > MAX_ATOMS ? -4 : 0;
}

static int parse_sequence(const char *s, int *pos, comp_t *c, int depth)
{
	int seen = 0;

	while (s[*pos] && s[*pos] != ')') {
		if (s[*pos] == '(') {
			comp_t sub;
			int n, rc;

			if (depth >= MAX_DEPTH)
				return -4;
			memset(&sub, 0, sizeof sub);
			(*pos)++;
			if ((rc = parse_sequence(s, pos, &sub, depth + 1)) != 0)
				return rc;
			if (s[*pos] != ')')
				return -2;
			(*pos)++;
			if ((n = read_count(s, pos)) < 0)
				return -4;
			if ((rc = merge_term(c, &sub, n)) != 0)
				return rc;
		} else if (is_upper(s[*pos])) {
			int start = *pos, idx, n, rc;
			comp_t one;

			(*pos)++;
			if (is_lower(s[*pos]))
				(*pos)++;
			idx = element_index(s + start, (size_t)(*pos - start));
			if (idx < 0)
				return -1;
			if ((n = read_count(s, pos)) < 0)
				return -4;
			memset(&one, 0, sizeof one);
			one.count[idx] = (unsigned short)n;
			one.total = n;
			if ((rc = merge_term(c, &one, 1)) != 0)
				return rc;
		} else {
			return -3;
		}
		seen = 1;
	}
	return seen ? 0 : -3;
}

/* A trailing charge, tight (`Ba2+`) or as its own token (`Ba(NO3)2 2+`). */
static int prepare(const char *text, char *body, int bodylen, int *charge)
{
	char buf[FORMULA_MAX_TEXT + 8];
	int rc, len;

	if ((rc = normalise(text, buf, (int)sizeof buf)) != 0)
		return rc;
	*charge = 0;
	for (len = 0; buf[len]; len++)
		if (buf[len] == ' ' || buf[len] == '\t') {
			char tail[8];
			int i, j = 0;

			if (len >= bodylen)
				return -4;
			memcpy(body, buf, (size_t)len);
			body[len] = '\0';
			for (i = len; buf[i] == ' ' || buf[i] == '\t'; i++)
				;
			for (; buf[i]; i++) {
				if (j >= (int)sizeof tail - 1)
					return -3;
				tail[j++] = buf[i];
			}
			tail[j] = '\0';
			if (j < 2 || (tail[j - 1] != '+' && tail[j - 1] != '-'))
				return -3;
			*charge = (tail[j - 1] == '+') ? 1 : -1;
			{
				int mag = 0, k;

				for (k = 0; k < j - 1; k++)
					if (!is_digit(tail[k]))
						return -3;
					else
						mag = mag * 10 + (tail[k] - '0');
				if (k > 0 && mag == 0)
					return -3;
				*charge *= (k > 0) ? mag : 1;
			}
			return strlen(body) ? 0 : -3;
		}
	if ((rc = normalise(text, body, bodylen)) != 0)
		return rc;
	len = (int)strlen(body);
	if (len >= 1 && (body[len - 1] == '+' || body[len - 1] == '-')) {
		int sign = body[len - 1] == '+' ? 1 : -1;
		int start = len - 1, mag = 0, k;

		while (start > 0 && is_digit(body[start - 1]))
			start--;
		for (k = start; k < len - 1; k++)
			mag = mag * 10 + (body[k] - '0');
		if (start == 0)
			return -3;
		*charge = sign * (k > start ? mag : 1);
		body[start] = '\0';
	}
	return body[0] ? 0 : -3;
}

static int parse_body(const char *s, comp_t *out)
{
	const char *p = s;
	int first = 1;

	memset(out, 0, sizeof *out);
	{
		int open = 0, close = 0;
		const char *q;

		for (q = s; *q; q++) {
			if (*q == '(')
				open++;
			else if (*q == ')')
				close++;
		}
		if (open != close)
			return -2;
	}
	while (*p) {
		const char *dot = strchr(p, '.');
		size_t seglen = dot ? (size_t)(dot - p) : strlen(p);
		char seg[FORMULA_MAX_TEXT + 8];
		comp_t sub;
		int mult = 1, i = 0, pos = 0, rc;

		if (seglen == 0 || seglen >= sizeof seg)
			return -3;
		memcpy(seg, p, seglen);
		seg[seglen] = '\0';
		if (seg[0] >= '0' && seg[0] <= '9') {
			if (first)
				return -3;
			mult = 0;
			while (is_digit(seg[i])) {
				mult = mult * 10 + (seg[i] - '0');
				i++;
				if (mult > MAX_COUNT)
					return -4;
			}
			if (!seg[i] || mult < 1)
				return -3;
		}
		memset(&sub, 0, sizeof sub);
		pos = i;
		if ((rc = parse_sequence(seg, &pos, &sub, 0)) != 0)
			return rc;
		if (seg[pos] != '\0')
			return -2;
		if ((rc = merge_term(out, &sub, mult)) != 0)
			return rc;
		first = 0;
		if (!dot)
			break;
		p = dot + 1;
	}
	return out->total > 0 ? 0 : -3;
}

static int lookup_name(const char *text, char *formula, int formulalen,
                       int *charge)
{
	char key[FORMULA_MAX_TEXT + 8];
	int i, j = 0, space = 0;

	/* Lowercase, with a space for every underscore or hyphen, which is the
	 * spelling a typed call can carry and the one the table stores. */
	for (i = 0; text[i] && j < (int)sizeof key - 1; i++) {
		char c = text[i];

		if (c == '_' || c == '-' || c == ' ') {
			if (j == 0 || space)
				continue;
			key[j++] = ' ';
			space = 1;
			continue;
		}
		if (c >= 'A' && c <= 'Z')
			c = (char)(c + 32);
		key[j++] = c;
		space = 0;
	}
	while (j > 0 && key[j - 1] == ' ')
		j--;
	key[j] = '\0';
	for (i = 0; i < CHEM_NUM_NAMES; i++) {
		if (strcmp(key, chem_names[i].name) != 0)
			continue;
		if ((int)strlen(chem_names[i].formula) >= formulalen)
			return -4;
		strcpy(formula, chem_names[i].formula);
		/* A table formula cannot carry a charge; the caller reads it. */
		*charge = 0;
		return 0;
	}
	return -1;
}

/* An argument as a formula body: the text itself, or a measured name. */
static int resolve_text(const char *text, char *formula, int formulalen,
                        int *charge)
{
	char body[FORMULA_MAX_TEXT + 8];
	comp_t ignored;
	int rc;

	if ((rc = prepare(text, body, (int)sizeof body, charge)) == 0 &&
	    parse_body(body, &ignored) == 0) {
		if ((int)strlen(body) >= formulalen)
			return -4;
		strcpy(formula, body);
		return 0;
	}
	if (text == NULL)
		return rc;
	return lookup_name(text, formula, formulalen, charge);
}

int formula_recognise(const char *text)
{
	char formula[FORMULA_MAX_TEXT + 8];
	int charge;

	return resolve_text(text, formula, (int)sizeof formula, &charge);
}

int formula_composition(const char *text, unsigned short *counts)
{
	char formula[FORMULA_MAX_TEXT + 8];
	comp_t comp;
	int charge, rc, i;

	if (counts == NULL)
		return -3;
	if ((rc = resolve_text(text, formula, (int)sizeof formula, &charge)) != 0)
		return rc;
	if ((rc = parse_body(formula, &comp)) != 0)
		return rc;
	for (i = 0; i < CHEM_NUM_ELEMENTS; i++)
		counts[i] = comp.count[i];
	return 0;
}

int formula_molar_mass_mg(const char *text, long long *mg)
{
	char formula[FORMULA_MAX_TEXT + 8];
	comp_t comp;
	long long total = 0;
	int charge, rc, i;

	if (mg == NULL)
		return -3;
	if ((rc = resolve_text(text, formula, (int)sizeof formula, &charge)) != 0)
		return rc;
	if ((rc = parse_body(formula, &comp)) != 0)
		return rc;
	for (i = 0; i < CHEM_NUM_ELEMENTS; i++)
		total += (long long)chem_elements[i].weight_mg * comp.count[i];
	if (total <= 0 || total > WIDE_LIMIT / 1000)
		return -4;
	*mg = total;
	return 0;
}

/* The anion part of a formula: its count and the magnitude of its charge. */
static int anion_of(const char *rest, int *count, int *charge)
{
	int i, open = -1, close = -1, n, pos;

	for (i = 0; rest[i]; i++) {
		if (rest[i] == '(') {
			if (open >= 0)
				return -4;
			open = i;
		} else if (rest[i] == ')') {
			close = i;
			break;
		}
	}
	if (open == 0) {
		char inner[FORMULA_MAX_TEXT + 8];
		size_t len;

		if (close < 0)
			return -2;
		len = (size_t)(close - 1);
		if (len == 0 || len >= sizeof inner)
			return -4;
		memcpy(inner, rest + 1, len);
		inner[len] = '\0';
		pos = close + 1;
		if ((n = read_count(rest, &pos)) < 0)
			return -4;
		if (rest[pos] != '\0')
			return -3;
		for (i = 0; i < CHEM_NUM_ANIONS; i++)
			if (strcmp(inner, chem_anions[i].formula) == 0) {
				*count = n;
				*charge = chem_anions[i].charge;
				return 0;
			}
		return -4;
	}
	if (open >= 0)
		return -4;
	/* One element and a count: a monatomic anion. */
	{
		char symbol[4];
		int end = 1;

		if (!is_upper(rest[0]))
			return -4;
		if (is_lower(rest[1]))
			end++;
		memcpy(symbol, rest, (size_t)end);
		symbol[end] = '\0';
		pos = end;
		if ((n = read_count(rest, &pos)) < 0)
			return -4;
		if (rest[pos] == '\0') {
			for (i = 0; i < CHEM_NUM_MONO_ANIONS; i++)
				if (strcmp(symbol, chem_mono_anions[i].formula) == 0) {
					*count = n;
					*charge = chem_mono_anions[i].charge;
					return 0;
				}
			return -4;
		}
	}
	/* Otherwise the whole remainder is a polyatomic ion as written. */
	for (i = 0; i < CHEM_NUM_ANIONS; i++)
		if (strcmp(rest, chem_anions[i].formula) == 0) {
			*count = 1;
			*charge = chem_anions[i].charge;
			return 0;
		}
	return -4;
}

int formula_ion_counts(const char *text, int *cations, int *anions)
{
	char formula[FORMULA_MAX_TEXT + 8];
	comp_t comp;
	const char *rest = NULL;
	int charge, rc, cation_charge = 0, cation_count = 1;
	int anion_count = 0, anion_charge = 0, i;

	if (cations == NULL || anions == NULL)
		return -3;
	if ((rc = resolve_text(text, formula, (int)sizeof formula, &charge)) != 0)
		return rc;
	if (charge != 0)
		return -4;
	if (strchr(formula, '.'))
		return -4;
	if ((rc = parse_body(formula, &comp)) != 0)
		return rc;
	if (formula[0] == '(') {
		int depth = 0, close = -1, pos;
		char key[FORMULA_MAX_TEXT + 8];
		size_t len;

		for (i = 0; formula[i]; i++) {
			if (formula[i] == '(')
				depth++;
			else if (formula[i] == ')' && --depth == 0) {
				close = i;
				break;
			}
		}
		if (close < 0)
			return -2;
		len = (size_t)(close - 1);
		if (len == 0 || len >= sizeof key)
			return -4;
		memcpy(key, formula + 1, len);
		key[len] = '\0';
		pos = close + 1;
		if ((cation_count = read_count(formula, &pos)) < 0)
			return -4;
		if (!formula[pos])
			return -4;
		rest = formula + pos;
		for (i = 0; i < CHEM_NUM_CATIONS; i++)
			if (strcmp(key, chem_cations[i].formula) == 0)
				cation_charge = chem_cations[i].charge;
		if (cation_charge == 0)
			return -4;
	} else {
		for (i = 0; i < CHEM_NUM_CATIONS; i++) {
			size_t n = strlen(chem_cations[i].formula);

			if (strncmp(formula, chem_cations[i].formula, n) == 0) {
				int pos = (int)n;

				cation_charge = chem_cations[i].charge;
				if ((cation_count = read_count(formula, &pos)) < 0)
					return -4;
				rest = formula + pos;
				break;
			}
		}
		if (rest == NULL) {
			int end = 1, idx, pos;

			if (!is_upper(formula[0]))
				return -4;
			if (is_lower(formula[1]))
				end++;
			idx = element_index(formula, (size_t)end);
			if (idx < 0)
				return -1;
			if (chem_elements[idx].cation_charge < 0)
				return -4;
			cation_charge = chem_elements[idx].cation_charge;
			pos = end;
			if ((cation_count = read_count(formula, &pos)) < 0)
				return -4;
			rest = formula + pos;
		}
	}
	if (rest == NULL || !*rest)
		return -4;
	if ((rc = anion_of(rest, &anion_count, &anion_charge)) != 0)
		return rc;
	if (cation_charge) {
		if (cation_count * cation_charge != anion_count * anion_charge)
			return -4;
	} else {
		int needed = anion_count * anion_charge;

		if (needed % cation_count != 0)
			return -4;
		cation_charge = needed / cation_count;
		if (cation_charge < 1 || cation_charge > 4)
			return -4;
	}
	*cations = cation_count;
	*anions = anion_count;
	return 0;
}

/* ---- the calls ---- */

static int solve_molar_mass_formula(const char *const *args, char *out,
                                    size_t outlen)
{
	long long mg;
	char value[48], buf[64];
	int rc;

	if ((rc = formula_molar_mass_mg(args[0], &mg)) != 0)
		return -3;
	if ((rc = dec4_render(mg, 1000, value, sizeof value)) != 0)
		return rc;
	snprintf(buf, sizeof buf, "%s g/mol", value);
	out_set(out, outlen, buf);
	return 0;
}

int formula_solve(const char *name, const char *const *args, int nargs,
                  char *out, size_t outlen)
{
	int rc;

	if (name == NULL)
		return 0;
	if (strcmp(name, "molar_mass_formula") != 0)
		return 0;
	if (nargs != 1)
		return -2;
	rc = solve_molar_mass_formula(args, out, outlen);
	return rc == 0 ? 1 : rc;
}

/* The formula as it was understood, for the working's first line. */
static void shown_formula(const char *text, char *out, size_t outlen)
{
	char formula[FORMULA_MAX_TEXT + 8];
	int charge;

	if (resolve_text(text, formula, (int)sizeof formula, &charge) == 0)
		out_set(out, outlen, formula);
	else
		out_set(out, outlen, text ? text : "");
}

/* M(CH3OH) = 12.01 + 4*1.008 + 16 */
static int molar_mass_line(const char *text, char *out, int outlen)
{
	char formula[FORMULA_MAX_TEXT + 8];
	comp_t comp;
	char *at = out, *end = out + outlen - 1;
	char piece[32];
	int charge, rc, i, terms = 0;

	if (resolve_text(text, formula, (int)sizeof formula, &charge) != 0)
		return -1;
	if (parse_body(formula, &comp) != 0)
		return -1;
	at += snprintf(at, (size_t)(end - at), "M(%s) = ", formula);
	for (i = 0; i < CHEM_NUM_ELEMENTS; i++) {
		if (!comp.count[i])
			continue;
		if (terms >= MAX_TERMS)
			return -1;
		if (dec4_render(chem_elements[i].weight_mg, 1000, piece,
		                sizeof piece) != 0)
			return -1;
		{
			char term[48];

			if (comp.count[i] == 1)
				snprintf(term, sizeof term, "%s%s",
				         terms ? " + " : "", piece);
			else
				snprintf(term, sizeof term, "%s%d*%s",
				         terms ? " + " : "", comp.count[i], piece);
			at += snprintf(at, (size_t)(end - at), "%s", term);
		}
		terms++;
	}
	(void)rc;
	return 0;
}

int formula_steps(const char *name, const char *const *args, int nargs,
                  const char *answer, char *out, int outlen)
{
	char first[160], second[64], value[48];
	long long mg;
	int rc;

	if (out == NULL || outlen < 2)
		return -1;
	out[0] = '\0';
	if (name == NULL || strcmp(name, "molar_mass_formula") != 0 || nargs != 1)
		return 0;
	if (molar_mass_line(args[0], first, (int)sizeof first) != 0) {
		shown_formula(args[0], first, sizeof first);
		snprintf(out, (size_t)outlen, "M(%s)", first);
	} else {
		snprintf(out, (size_t)outlen, "%s", first);
	}
	if ((rc = formula_molar_mass_mg(args[0], &mg)) == 0 &&
	    dec4_render(mg, 1000, value, sizeof value) == 0) {
		snprintf(second, sizeof second, "\n= %s g/mol", value);
		if (strlen(out) + strlen(second) < (size_t)outlen)
			strcat(out, second);
	}
	if (answer && answer[0]) {
		char line[96];

		snprintf(line, sizeof line, "\n= %s", answer);
		if (strlen(out) + strlen(line) < (size_t)outlen)
			strcat(out, line);
	}
	return 1;
}
