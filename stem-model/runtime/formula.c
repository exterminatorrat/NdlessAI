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
#include <stdarg.h>
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

/*
 * The arithmetic is exact rationals, like every other solver, and it refuses
 * when a product would leave the int64 the device holds rather than wrapping
 * into a plausible-looking number. Cross-reduction before each multiply is
 * what keeps a Ksp of a sparingly soluble salt inside that range.
 */
typedef struct {
	long long n, d;
} frac;

long long gcd_ll(long long a, long long b);

static int frac_set(frac *r, long long n, long long d)
{
	long long g;

	if (d == 0)
		return -4;
	if (d < 0) {
		n = -n;
		d = -d;
	}
	if (n == 0) {
		r->n = 0;
		r->d = 1;
		return 0;
	}
	g = gcd_ll(n, d);
	r->n = n / g;
	r->d = d / g;
	return 0;
}

static int frac_mul(frac *r, const frac *a, const frac *b)
{
	long long g1 = gcd_ll(a->n, b->d), g2 = gcd_ll(b->n, a->d);
	long long an = a->n / g1, ad = a->d / g2;
	long long bn = b->n / g2, bd = b->d / g1;
	long long an_abs = an < 0 ? -an : an;
	long long bn_abs = bn < 0 ? -bn : bn;

	if (an_abs != 0 && bn_abs > WIDE_LIMIT / an_abs)
		return -4;
	if (ad != 0 && bd > WIDE_LIMIT / ad)
		return -4;
	return frac_set(r, an * bn, ad * bd);
}

static int frac_mul_ll(frac *r, const frac *a, long long k)
{
	frac b;

	b.n = k;
	b.d = 1;
	return frac_mul(r, a, &b);
}

static int frac_div_ll(frac *r, const frac *a, long long k)
{
	frac b;

	b.n = 1;
	b.d = k;
	return frac_mul(r, a, &b);
}

static int frac_add(frac *r, const frac *a, const frac *b)
{
	long long g = gcd_ll(a->d, b->d);
	long long ad = a->d / g, bd = b->d / g;
	long long an_abs = a->n < 0 ? -a->n : a->n;
	long long bn_abs = b->n < 0 ? -b->n : b->n;

	if (an_abs != 0 && bd > WIDE_LIMIT / an_abs)
		return -4;
	if (bn_abs != 0 && ad > WIDE_LIMIT / bn_abs)
		return -4;
	if (ad != 0 && b->d > WIDE_LIMIT / ad)
		return -4;
	return frac_set(r, a->n * bd + b->n * ad, ad * b->d);
}

static int arg_frac(const char *text, frac *f)
{
	rat r;
	int rc = parse_rat(text, &r, NULL);

	if (rc != 0)
		return rc;
	return frac_set(f, r.n, r.d);
}

static int grams_from_mass(const char *mass, frac *out)
{
	frac m;

	if (arg_frac(mass, &m) != 0)
		return -3;
	return frac_mul_ll(out, &m, 1000);
}

static int answer_fraction(const frac *value, const char *unit, char *out,
                           size_t outlen)
{
	char piece[48], buf[64];
	int rc;

	if ((rc = dec4_render(value->n, value->d, piece, sizeof piece)) != 0)
		return rc;
	if (unit && unit[0])
		snprintf(buf, sizeof buf, "%s %s", piece, unit);
	else
		snprintf(buf, sizeof buf, "%s", piece);
	out_set(out, outlen, buf);
	return 0;
}

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

static int solve_moles_from_mass_formula(const char *const *args, char *out,
                                         size_t outlen)
{
	long long mg;
	frac n;
	int rc;

	if (formula_molar_mass_mg(args[1], &mg) != 0)
		return -3;
	if ((rc = grams_from_mass(args[0], &n)) != 0)
		return rc;
	if ((rc = frac_div_ll(&n, &n, mg)) != 0)
		return rc;
	return answer_fraction(&n, "mol", out, outlen);
}

static int solve_mass_from_moles_formula(const char *const *args, char *out,
                                         size_t outlen)
{
	long long mg;
	frac n;
	int rc;

	if (formula_molar_mass_mg(args[1], &mg) != 0)
		return -3;
	if ((rc = arg_frac(args[0], &n)) != 0)
		return -3;
	if ((rc = frac_mul_ll(&n, &n, mg)) != 0)
		return rc;
	if ((rc = frac_div_ll(&n, &n, 1000)) != 0)
		return rc;
	return answer_fraction(&n, "g", out, outlen);
}

static int solve_ions_from_moles_formula(const char *const *args, char *out,
                                         size_t outlen)
{
	int cations, anions, rc;
	frac n;

	if (formula_ion_counts(args[1], &cations, &anions) != 0)
		return -3;
	if ((rc = arg_frac(args[0], &n)) != 0)
		return -3;
	if ((rc = frac_mul_ll(&n, &n, cations + anions)) != 0)
		return rc;
	return answer_fraction(&n, "mol ions", out, outlen);
}

static int solve_ksp_from_solubility_formula(const char *const *args, char *out,
                                             size_t outlen)
{
	int cations, anions, i, rc;
	frac s, cp, ap, ksp;

	if (formula_ion_counts(args[1], &cations, &anions) != 0)
		return -3;
	if ((rc = arg_frac(args[0], &s)) != 0)
		return -3;
	if (s.n <= 0)
		return -4;
	for (i = 0; i < 2; i++) {
		int count = i == 0 ? cations : anions;
		frac *term = i == 0 ? &cp : &ap;
		frac side;

		if ((rc = frac_mul_ll(&side, &s, count)) != 0)
			return rc;
		term->n = 1;
		term->d = 1;
		for (int k = 0; k < count; k++)
			if ((rc = frac_mul(term, term, &side)) != 0)
				return rc;
	}
	if ((rc = frac_mul(&ksp, &cp, &ap)) != 0)
		return rc;
	return answer_fraction(&ksp, "", out, outlen);
}

static int solve_mole_fraction_formula(const char *const *args, char *out,
                                       size_t outlen)
{
	long long mg_a, mg_b;
	frac n_a, n_b, total;
	int rc;

	if (formula_molar_mass_mg(args[1], &mg_a) != 0)
		return -3;
	if (formula_molar_mass_mg(args[3], &mg_b) != 0)
		return -3;
	if ((rc = grams_from_mass(args[0], &n_a)) != 0 ||
	    (rc = frac_div_ll(&n_a, &n_a, mg_a)) != 0)
		return rc;
	if ((rc = grams_from_mass(args[2], &n_b)) != 0 ||
	    (rc = frac_div_ll(&n_b, &n_b, mg_b)) != 0)
		return rc;
	if ((rc = frac_add(&total, &n_a, &n_b)) != 0 || total.n == 0)
		return -4;
	{
		frac inv;

		inv.n = total.d;
		inv.d = total.n;
		if (inv.d < 0) {
			inv.n = -inv.n;
			inv.d = -inv.d;
		}
		if ((rc = frac_mul(&n_a, &n_a, &inv)) != 0)
			return rc;
	}
	return answer_fraction(&n_a, "", out, outlen);
}

static int solve_stoich_mass_formula(const char *const *args, char *out,
                                     size_t outlen)
{
	long long mg_given, mg_product;
	frac moles, ratio, m;
	int rc;

	if (formula_molar_mass_mg(args[1], &mg_given) != 0)
		return -3;
	if (formula_molar_mass_mg(args[3], &mg_product) != 0)
		return -3;
	if ((rc = grams_from_mass(args[0], &moles)) != 0 ||
	    (rc = frac_div_ll(&moles, &moles, mg_given)) != 0)
		return rc;
	if ((rc = arg_frac(args[2], &ratio)) != 0)
		return -3;
	if ((rc = frac_mul(&m, &moles, &ratio)) != 0 ||
	    (rc = frac_mul_ll(&m, &m, mg_product)) != 0 ||
	    (rc = frac_div_ll(&m, &m, 1000)) != 0)
		return rc;
	return answer_fraction(&m, "g", out, outlen);
}

static const struct {
	const char *name;
	unsigned char arity;
	int (*solve)(const char *const *args, char *out, size_t outlen);
} calls[] = {
	{"molar_mass_formula", 1, solve_molar_mass_formula},
	{"moles_from_mass_formula", 2, solve_moles_from_mass_formula},
	{"mass_from_moles_formula", 2, solve_mass_from_moles_formula},
	{"ions_from_moles_formula", 2, solve_ions_from_moles_formula},
	{"ksp_from_solubility_formula", 2, solve_ksp_from_solubility_formula},
	{"mole_fraction_formula", 4, solve_mole_fraction_formula},
	{"stoich_mass_formula", 4, solve_stoich_mass_formula},
};

int formula_solve(const char *name, const char *const *args, int nargs,
                  char *out, size_t outlen)
{
	size_t i;

	if (name == NULL)
		return 0;
	for (i = 0; i < sizeof calls / sizeof calls[0]; i++) {
		int rc;

		if (strcmp(name, calls[i].name) != 0)
			continue;
		if (nargs != (int)calls[i].arity)
			return -2;
		rc = calls[i].solve(args, out, outlen);
		return rc == 0 ? 1 : rc;
	}
	return 0;
}

/* ---- the working ---- */

static int append(char *out, int outlen, const char *s)
{
	size_t have = strlen(out), add = strlen(s);

	if (have + add >= (size_t)outlen)
		return -1;
	memcpy(out + have, s, add + 1);
	return 0;
}

static int appendf(char *out, int outlen, const char *fmt, ...)
{
	char line[160];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(line, sizeof line, fmt, ap);
	va_end(ap);
	return append(out, outlen, line);
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

/* M(CH3OH) = 12.01 + 4*1.008 + 16, in the order the formula is written. */
static int molar_mass_line(const char *text, char *out, int outlen)
{
	char formula[FORMULA_MAX_TEXT + 8];
	comp_t comp;
	unsigned char used[CHEM_NUM_ELEMENTS];
	char *at = out, *end = out + outlen - 1;
	const char *p;
	int charge, terms = 0;

	if (resolve_text(text, formula, (int)sizeof formula, &charge) != 0)
		return -1;
	if (parse_body(formula, &comp) != 0)
		return -1;
	memset(used, 0, sizeof used);
	at += snprintf(at, (size_t)(end - at), "M(%s) = ", formula);
	for (p = formula; *p; ) {
		int start, end_at, idx;
		char piece[32], term[48];

		if (!is_upper(*p)) {
			p++;
			continue;
		}
		start = (int)(p - formula);
		end_at = start + 1;
		if (is_lower(formula[end_at]))
			end_at++;
		p = formula + end_at;
		idx = element_index(formula + start, (size_t)(end_at - start));
		if (idx < 0 || used[idx] || !comp.count[idx])
			continue;
		used[idx] = 1;
		if (terms >= MAX_TERMS)
			return -1;
		if (dec4_render(chem_elements[idx].weight_mg, 1000, piece,
		                sizeof piece) != 0)
			return -1;
		if (comp.count[idx] == 1)
			snprintf(term, sizeof term, "%s%s", terms ? " + " : "",
			         piece);
		else
			snprintf(term, sizeof term, "%s%d*%s",
			         terms ? " + " : "", comp.count[idx], piece);
		at += snprintf(at, (size_t)(end - at), "%s", term);
		terms++;
	}
	return terms ? 0 : -1;
}

/* "M(CH3OH) = 32.04 g/mol", or the formula alone when the line will not fit. */
static int mass_line(const char *text, char *out, int outlen)
{
	char formula[FORMULA_MAX_TEXT + 8];
	char line[128], value[32];
	long long mg;

	if (formula_molar_mass_mg(text, &mg) != 0 ||
	    dec4_render(mg, 1000, value, sizeof value) != 0)
		return -1;
	shown_formula(text, formula, sizeof formula);
	snprintf(line, sizeof line, "M(%s) = %s g/mol", formula, value);
	out_set(out, (size_t)outlen, line);
	return 0;
}

static int answer_line(char *out, int outlen, const char *answer)
{
	if (answer == NULL || !answer[0])
		return 0;
	return appendf(out, outlen, "\n= %.120s", answer);
}

static int steps_molar_mass(const char *text, const char *answer, char *out,
                            int outlen)
{
	if (molar_mass_line(text, out, outlen) != 0) {
		char formula[FORMULA_MAX_TEXT + 8], line[96];

		shown_formula(text, formula, sizeof formula);
		snprintf(line, sizeof line, "M(%s)", formula);
		out_set(out, (size_t)outlen, line);
	}
	return answer_line(out, outlen, answer);
}

static int steps_moles_from_mass(const char *const *args, const char *answer,
                                 char *out, int outlen)
{
	char line[120], value[32];
	long long mg;
	char formula[FORMULA_MAX_TEXT + 8];

	if (formula_molar_mass_mg(args[1], &mg) != 0)
		return -1;
	shown_formula(args[1], formula, sizeof formula);
	snprintf(line, sizeof line, "M(%s) = ", formula);
	if (append(out, outlen, line) < 0 ||
	    dec4_render(mg, 1000, value, sizeof value) != 0 ||
	    append(out, outlen, value) < 0 ||
	    append(out, outlen, " g/mol") < 0)
		return -1;
	snprintf(line, sizeof line, "\nn = m/M = %.24s/", args[0]);
	if (append(out, outlen, line) < 0 || append(out, outlen, value) < 0)
		return -1;
	return answer_line(out, outlen, answer);
}

static int steps_mass_from_moles(const char *const *args, const char *answer,
                                 char *out, int outlen)
{
	char line[120], value[32];
	long long mg;
	char formula[FORMULA_MAX_TEXT + 8];

	if (formula_molar_mass_mg(args[1], &mg) != 0)
		return -1;
	shown_formula(args[1], formula, sizeof formula);
	snprintf(line, sizeof line, "M(%s) = ", formula);
	if (append(out, outlen, line) < 0 ||
	    dec4_render(mg, 1000, value, sizeof value) != 0 ||
	    append(out, outlen, value) < 0 ||
	    append(out, outlen, " g/mol") < 0)
		return -1;
	if (appendf(out, outlen, "\nm = n*M = %.24s*%s", args[0], value) < 0)
		return -1;
	return answer_line(out, outlen, answer);
}

static int ion_line(const char *formula_arg, char *out, int outlen)
{
	int cations, anions;
	char formula[FORMULA_MAX_TEXT + 8], line[120];

	if (formula_ion_counts(formula_arg, &cations, &anions) != 0)
		return -1;
	shown_formula(formula_arg, formula, sizeof formula);
	snprintf(line, sizeof line, "%s: %d cation%s + %d anion%s", formula,
	         cations, cations == 1 ? "" : "s", anions, anions == 1 ? "" : "s");
	return append(out, outlen, line);
}

static int steps_ions_from_moles(const char *const *args, const char *answer,
                                 char *out, int outlen)
{
	int cations, anions;

	if (formula_ion_counts(args[1], &cations, &anions) != 0)
		return -1;
	if (ion_line(args[1], out, outlen) < 0)
		return -1;
	if (appendf(out, outlen, "\nn = %.24s*", args[0]) < 0)
		return -1;
	{
		char count[16];

		snprintf(count, sizeof count, "%d", cations + anions);
		if (append(out, outlen, count) < 0)
			return -1;
	}
	return answer_line(out, outlen, answer);
}

static int steps_ksp(const char *const *args, const char *answer, char *out,
                     int outlen)
{
	int cations, anions;
	char formula[FORMULA_MAX_TEXT + 8], line[160];

	if (formula_ion_counts(args[1], &cations, &anions) != 0)
		return -1;
	if (ion_line(args[1], out, outlen) < 0)
		return -1;
	shown_formula(args[1], formula, sizeof formula);
	snprintf(line, sizeof line,
	         "\nKsp = (%d*%.24s)^%d*(%d*%.24s)^%d", cations, args[0], cations,
	         anions, args[0], anions);
	if (append(out, outlen, line) < 0)
		return -1;
	return answer_line(out, outlen, answer);
}

static int steps_mole_fraction(const char *const *args, const char *answer,
                               char *out, int outlen)
{
	char first[96], second[96], line[160];

	if (mass_line(args[1], first, sizeof first) < 0 ||
	    mass_line(args[3], second, sizeof second) < 0)
		return -1;
	if (append(out, outlen, first) < 0 || append(out, outlen, "\n") < 0 ||
	    append(out, outlen, second) < 0)
		return -1;
	{
		long long mg_a, mg_b;
		frac n_a, n_b, total;
		char va[32], vb[32];

		if (formula_molar_mass_mg(args[1], &mg_a) != 0 ||
		    formula_molar_mass_mg(args[3], &mg_b) != 0)
			return -1;
		if (grams_from_mass(args[0], &n_a) != 0 ||
		    frac_div_ll(&n_a, &n_a, mg_a) != 0 ||
		    grams_from_mass(args[2], &n_b) != 0 ||
		    frac_div_ll(&n_b, &n_b, mg_b) != 0 ||
		    frac_add(&total, &n_a, &n_b) != 0)
			return -1;
		if (dec4_render(n_a.n, n_a.d, va, sizeof va) != 0 ||
		    dec4_render(n_b.n, n_b.d, vb, sizeof vb) != 0)
			return -1;
		snprintf(line, sizeof line, "\nnA = %s mol, nB = %s mol", va, vb);
		if (append(out, outlen, line) < 0)
			return -1;
	}
	if (append(out, outlen, "\nx = nA/(nA+nB)") < 0)
		return -1;
	return answer_line(out, outlen, answer);
}

static int steps_stoich_mass(const char *const *args, const char *answer, char *out,
                             int outlen)
{
	char first[96], second[96], product[32], given[32];
	long long mg_product, mg_given;

	if (mass_line(args[1], first, sizeof first) < 0 ||
	    mass_line(args[3], second, sizeof second) < 0)
		return -1;
	if (formula_molar_mass_mg(args[3], &mg_product) != 0 ||
	    dec4_render(mg_product, 1000, product, sizeof product) != 0 ||
	    formula_molar_mass_mg(args[1], &mg_given) != 0 ||
	    dec4_render(mg_given, 1000, given, sizeof given) != 0)
		return -1;
	if (append(out, outlen, first) < 0 || append(out, outlen, "\n") < 0 ||
	    append(out, outlen, second) < 0)
		return -1;
	if (appendf(out, outlen, "\nm = %.24s/", args[0]) < 0 ||
	    append(out, outlen, given) < 0 ||
	    appendf(out, outlen, "*%.24s*", args[2]) < 0 ||
	    append(out, outlen, product) < 0)
		return -1;
	return answer_line(out, outlen, answer);
}

int formula_steps(const char *name, const char *const *args, int nargs,
                  const char *answer, char *out, int outlen)
{
	if (out == NULL || outlen < 2)
		return -1;
	out[0] = '\0';
	if (name == NULL)
		return 0;
	if (strcmp(name, "molar_mass_formula") == 0 && nargs == 1)
		return steps_molar_mass(args[0], answer, out, outlen) == 0 ? 1 : -1;
	if (strcmp(name, "moles_from_mass_formula") == 0 && nargs == 2)
		return steps_moles_from_mass(args, answer, out, outlen) == 0 ? 1 : -1;
	if (strcmp(name, "mass_from_moles_formula") == 0 && nargs == 2)
		return steps_mass_from_moles(args, answer, out, outlen) == 0 ? 1 : -1;
	if (strcmp(name, "ions_from_moles_formula") == 0 && nargs == 2)
		return steps_ions_from_moles(args, answer, out, outlen) == 0 ? 1 : -1;
	if (strcmp(name, "ksp_from_solubility_formula") == 0 && nargs == 2)
		return steps_ksp(args, answer, out, outlen) == 0 ? 1 : -1;
	if (strcmp(name, "mole_fraction_formula") == 0 && nargs == 4)
		return steps_mole_fraction(args, answer, out, outlen) == 0 ? 1 : -1;
	if (strcmp(name, "stoich_mass_formula") == 0 && nargs == 4)
		return steps_stoich_mass(args, answer, out, outlen) == 0 ? 1 : -1;
	(void)nargs;
	return 0;
}
