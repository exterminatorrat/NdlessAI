#include <stdio.h>
#include <string.h>

#include "grounding.h"
#include "quantity.h"
#include "reltable.h"
#include "solvers_subject.h"

/* Revised 2026-11-17: unitless zero must not block multiplicative conversions. */

/*
 * One unit, under every spelling a question uses for it. The first spelling
 * is the canonical name that relations/param_units.py writes, so it never
 * contains a space. Spellings that read as ordinary words are left out on
 * purpose: "in" is a preposition, and a bare "C" or "F" could be a coulomb, a
 * farad or a temperature.
 *
 * `num/den` is the unit in its kind's base unit, exactly; den 0 marks a unit
 * whose rate to the others is not a small rational (radians, electronvolts),
 * which can be matched but never converted. `off` is added after scaling and
 * is only non-zero for Celsius and Fahrenheit.
 */
typedef struct {
	const char *spellings;   /* '|'-separated, canonical first */
	unsigned char kind;
	long long num, den;
	long long off_num, off_den;
} unit_t;

enum {
	K_LEN = 1, K_MASS, K_AMU, K_TIME, K_SPEED, K_ACCEL, K_TEMP, K_ANGLE, K_VOL,
	K_AREA, K_PRES, K_ENERGY, K_EV, K_FORCE, K_POWER, K_CHARGE, K_CAP, K_RES,
	K_VOLT, K_CURR, K_FREQ, K_AMOUNT, K_CONC, K_MOLAR_MASS, K_DENSITY, K_TESLA,
	K_DB, K_INTENSITY, K_STIFFNESS, K_LIN_DENSITY, K_LATENT, K_MOLAR_ENERGY,
	K_ENTROPY
};

#define DEG "\xc2\xb0"
#define MICRO "\xc2\xb5"
#define OMEGA "\xce\xa9"
#define SQ "\xc2\xb2"
#define CU "\xc2\xb3"

static const unit_t UNITS[] = {
	{"m|metre|metres|meter|meters", K_LEN, 1, 1, 0, 0},
	{"cm|centimetre|centimetres|centimeter|centimeters", K_LEN, 1, 100, 0, 0},
	{"mm|millimetre|millimetres|millimeter|millimeters", K_LEN, 1, 1000, 0, 0},
	{"km|kilometre|kilometres|kilometer|kilometers", K_LEN, 1000, 1, 0, 0},
	{"nm|nanometre|nanometres|nanometer|nanometers", K_LEN, 1, 1000000000, 0, 0},
	{"inch|inches", K_LEN, 254, 10000, 0, 0},
	{"ft|foot|feet", K_LEN, 3048, 10000, 0, 0},
	{"mi|mile|miles", K_LEN, 1609344, 1000, 0, 0},
	{"kg|kilogram|kilograms|kgs", K_MASS, 1, 1, 0, 0},
	{"g|gram|grams|gramme|grammes", K_MASS, 1, 1000, 0, 0},
	{"mg|milligram|milligrams", K_MASS, 1, 1000000, 0, 0},
	{"lb|lbs|pound|pounds", K_MASS, 45359237, 100000000, 0, 0},
	{"u|amu", K_AMU, 1, 1, 0, 0},
	{"s|sec|secs|second|seconds", K_TIME, 1, 1, 0, 0},
	{"ms|millisecond|milliseconds", K_TIME, 1, 1000, 0, 0},
	{"min|mins|minute|minutes", K_TIME, 60, 1, 0, 0},
	{"hours|hour|h|hr|hrs", K_TIME, 3600, 1, 0, 0},
	{"days|day", K_TIME, 86400, 1, 0, 0},
	/* The Julian year, 365.25 days: the year physics and astronomy use. */
	{"years|year|yr|yrs", K_TIME, 31557600, 1, 0, 0},
	{"m/s|mps|metres per second|meters per second", K_SPEED, 1, 1, 0, 0},
	{"km/h|kph|kmh|km/hr|kilometres per hour|kilometers per hour|km per hour",
	 K_SPEED, 5, 18, 0, 0},
	{"mph|mi/h|miles per hour", K_SPEED, 44704, 100000, 0, 0},
	{"m/s^2|m/s2|m/s" SQ "|m/s/s|metres per second squared|"
	 "meters per second squared", K_ACCEL, 1, 1, 0, 0},
	{"K|kelvin|kelvins|Kelvin", K_TEMP, 1, 1, 0, 0},
	{"degC|" DEG "C|degrees C|degrees Celsius|degree Celsius|degrees celsius|"
	 "deg C|Celsius|celsius", K_TEMP, 1, 1, 27315, 100},
	{"degF|" DEG "F|degrees F|degrees Fahrenheit|degree Fahrenheit|"
	 "degrees fahrenheit|deg F|Fahrenheit|fahrenheit", K_TEMP, 5, 9, 45967, 180},
	{"degrees|degree|deg|" DEG, K_ANGLE, 1, 1, 0, 0},
	{"rad|radian|radians", K_ANGLE, 1, 0, 0, 0},
	{"L|litre|litres|liter|liters", K_VOL, 1, 1, 0, 0},
	{"mL|ml|millilitre|millilitres|milliliter|milliliters|cm^3|cm3|cm" CU "|cc",
	 K_VOL,
	 1, 1000, 0, 0},
	{"m^3|m3|m" CU "|cubic metres|cubic meters", K_VOL, 1000, 1, 0, 0},
	{"m^2|m2|m" SQ "|square metres|square meters", K_AREA, 1, 1, 0, 0},
	{"cm^2|cm2|cm" SQ "|square centimetres|square centimeters", K_AREA, 1, 10000, 0, 0},
	{"mm^2|mm2", K_AREA, 1, 1000000, 0, 0},
	{"km^2|km2", K_AREA, 1000000, 1, 0, 0},
	{"Pa|pascal|pascals", K_PRES, 1, 1, 0, 0},
	{"kPa|kilopascal|kilopascals", K_PRES, 1000, 1, 0, 0},
	{"atm|atmosphere|atmospheres", K_PRES, 101325, 1, 0, 0},
	{"mmHg|mm Hg|torr|Torr", K_PRES, 101325, 760, 0, 0},
	{"bar", K_PRES, 100000, 1, 0, 0},
	{"J|joule|joules", K_ENERGY, 1, 1, 0, 0},
	{"kJ|kilojoule|kilojoules", K_ENERGY, 1000, 1, 0, 0},
	{"cal|calorie|calories", K_ENERGY, 4184, 1000, 0, 0},
	{"kcal|kilocalorie|kilocalories", K_ENERGY, 4184, 1, 0, 0},
	/* eV against J needs 1.602176634e-19, which no 64-bit rational here can
	 * hold, so electronvolts are their own kind: a joule in an eV slot is
	 * refused rather than converted. */
	{"eV|electronvolt|electronvolts|electron volts", K_EV, 1, 1, 0, 0},
	{"keV", K_EV, 1000, 1, 0, 0},
	{"MeV", K_EV, 1000000, 1, 0, 0},
	{"N|newton|newtons", K_FORCE, 1, 1, 0, 0},
	{"kN|kilonewton|kilonewtons", K_FORCE, 1000, 1, 0, 0},
	{"W|watt|watts", K_POWER, 1, 1, 0, 0},
	{"kW|kilowatt|kilowatts", K_POWER, 1000, 1, 0, 0},
	{"coulombs|coulomb", K_CHARGE, 1, 1, 0, 0},
	{"mC|millicoulomb|millicoulombs", K_CHARGE, 1, 1000, 0, 0},
	{"uC|" MICRO "C|microcoulomb|microcoulombs", K_CHARGE, 1, 1000000, 0, 0},
	{"nC|nanocoulomb|nanocoulombs", K_CHARGE, 1, 1000000000, 0, 0},
	{"farads|farad", K_CAP, 1, 1, 0, 0},
	{"uF|" MICRO "F|microfarad|microfarads", K_CAP, 1, 1000000, 0, 0},
	{"nF|nanofarad|nanofarads", K_CAP, 1, 1000000000, 0, 0},
	{"pF|picofarad|picofarads", K_CAP, 1, 1000000000000LL, 0, 0},
	{"ohm|ohms|Ohm|Ohms|" OMEGA, K_RES, 1, 1, 0, 0},
	{"kohm|kohms|kilohm|kilohms|k" OMEGA, K_RES, 1000, 1, 0, 0},
	{"V|volt|volts", K_VOLT, 1, 1, 0, 0},
	{"mV|millivolt|millivolts", K_VOLT, 1, 1000, 0, 0},
	{"kV|kilovolt|kilovolts", K_VOLT, 1000, 1, 0, 0},
	{"A|amp|amps|ampere|amperes", K_CURR, 1, 1, 0, 0},
	{"mA|milliamp|milliamps", K_CURR, 1, 1000, 0, 0},
	{"Hz|hertz", K_FREQ, 1, 1, 0, 0},
	{"kHz", K_FREQ, 1000, 1, 0, 0},
	{"MHz", K_FREQ, 1000000, 1, 0, 0},
	{"mol|mole|moles", K_AMOUNT, 1, 1, 0, 0},
	{"mmol|millimole|millimoles", K_AMOUNT, 1, 1000, 0, 0},
	{"M|mol/L|mol/l|molar|mol per litre|mol per liter|moles per litre|"
	 "moles per liter", K_CONC, 1, 1, 0, 0},
	{"mM|mmol/L|millimolar", K_CONC, 1, 1000, 0, 0},
	{"g/mol|grams per mole|g per mole", K_MOLAR_MASS, 1, 1, 0, 0},
	{"kg/mol|kg per mole", K_MOLAR_MASS, 1000, 1, 0, 0},
	{"kg/m^3|kg/m3|kg/m" CU "|kg per cubic metre|kg per cubic meter", K_DENSITY, 1, 1, 0, 0},
	{"g/cm^3|g/cm3|g/mL|g/ml|g/cc", K_DENSITY, 1000, 1, 0, 0},
	{"g/L", K_DENSITY, 1, 1, 0, 0},
	{"T|tesla|teslas", K_TESLA, 1, 1, 0, 0},
	{"mT", K_TESLA, 1, 1000, 0, 0},
	{"dB|decibel|decibels", K_DB, 1, 1, 0, 0},
	{"W/m^2|W/m2|W/m" SQ, K_INTENSITY, 1, 1, 0, 0},
	{"N/m|newtons per metre|newtons per meter", K_STIFFNESS, 1, 1, 0, 0},
	{"kg/m|kg per metre|kg per meter", K_LIN_DENSITY, 1, 1, 0, 0},
	{"J/g|kJ/kg|joules per gram", K_LATENT, 1, 1, 0, 0},
	{"J/kg", K_LATENT, 1, 1000, 0, 0},
	{"kJ/mol|kJ per mole|kJ per mol", K_MOLAR_ENERGY, 1, 1, 0, 0},
	{"J/mol", K_MOLAR_ENERGY, 1, 1000, 0, 0},
	{"J/K|joules per kelvin", K_ENTROPY, 1, 1, 0, 0},
	{"kJ/K", K_ENTROPY, 1000, 1, 0, 0},
};

#define N_UNITS ((int)(sizeof UNITS / sizeof UNITS[0]))

static int is_digit(char c)
{
	return c >= '0' && c <= '9';
}

static int is_alpha(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

/* The canonical name: the spelling before the first '|'. */
static void canonical(const unit_t *u, char *out, size_t outlen)
{
	size_t n = strcspn(u->spellings, "|");

	if (n >= outlen)
		n = outlen - 1;
	memcpy(out, u->spellings, n);
	out[n] = '\0';
}

/*
 * Length of the longest spelling of u that starts at `at` and ends at a word
 * boundary, or 0. "m" must not match the front of "m/s" or "meters", and a
 * trailing "/", "^", digit or non-ASCII byte means the unit goes on: "m/s" is
 * not the whole of "m/s\xc2\xb2", whose superscript is a UTF-8 sequence.
 */
static int spelled_at(const unit_t *u, const char *at)
{
	const char *s = u->spellings;
	int best = 0;

	while (*s) {
		size_t n = strcspn(s, "|");

		/* at[n] is only read once the first n characters matched, which
		 * proves the text is at least that long. */
		if (strncmp(at, s, n) == 0 && (int)n > best) {
			char next = at[n];

			if (!is_alpha(next) && !is_digit(next) && next != '/' &&
			    next != '^' && next != '_' && !((unsigned char)next & 0x80))
				best = (int)n;
		}
		s += n;
		if (*s == '|')
			s++;
	}
	return best;
}

/*
 * Whether the text after a matched spelling carries on the same unit. A
 * compound this table does not spell in full is unread, never read as its
 * first word: "metres per second" is not metres, "m / s^2" is not m, and
 * "m s^-1" or "m s\xe2\x81\xbb\xc2\xb9" is not m either.
 */
static int continues(const char *after)
{
	const char *p = after;

	while (*p == ' ')
		p++;
	if (*p == '/' || strncmp(p, "per ", 4) == 0)
		return p > after || *p == '/';
	if (p == after)
		return 0;
	/* A spaced second unit with a negative or superscript exponent. */
	while (is_alpha(*p))
		p++;
	return p > after && is_alpha(p[-1]) &&
	       (*p == '^' || (*p == '-' && is_digit(p[1])) ||
	        ((unsigned char)*p & 0x80));
}

static const unit_t *unit_at(const char *at)
{
	const unit_t *best = NULL;
	int best_len = 0;

	for (int i = 0; i < N_UNITS; i++) {
		int n = spelled_at(&UNITS[i], at);

		if (n > best_len) {
			best_len = n;
			best = &UNITS[i];
		}
	}
	if (best && continues(at + best_len))
		return NULL;
	return best;
}

static const unit_t *unit_named(const char *name)
{
	size_t n = strlen(name);
	const unit_t *u = unit_at(name);

	/* The whole name, not a prefix of it. */
	return u && spelled_at(u, name) == (int)n ? u : NULL;
}

static const unit_t *unit_after(const char *end, int *span)
{
	const char *at = end;
	const unit_t *u;

	/* "5 kg", "5kg" and the attributive "a 5-kg block" all write a unit. */
	if (*at == '-' && is_alpha(at[1]))
		at++;
	else
		while (*at == ' ')
			at++;
	u = unit_at(at);
	if (u && span)
		*span = (int)(at - end) + spelled_at(u, at);
	return u;
}

const char *quantity_unit_after(const char *end, int *span)
{
	static char name[24];
	const unit_t *u = unit_after(end, span);

	if (!u)
		return NULL;
	canonical(u, name, sizeof name);
	return name;
}

int quantity_unit_known(const char *name)
{
	return unit_named(name) != NULL;
}

/* ---- exact arithmetic ---- */

/* gcd_ll is the solvers' own; every call here has a non-zero denominator in
 * it, so it never sees gcd(0, 0). */

/* r = a*b and r = a+b on reduced rationals; -1 on overflow. */
static int q_mul(rat *r, rat a, rat b)
{
	long long g1 = gcd_ll(a.n, b.d), g2 = gcd_ll(b.n, a.d);
	long long n, d;

	if (__builtin_mul_overflow(a.n / g1, b.n / g2, &n) ||
	    __builtin_mul_overflow(a.d / g2, b.d / g1, &d))
		return -1;
	r->n = n;
	r->d = d;
	return 0;
}

static int q_add(rat *r, rat a, rat b)
{
	long long g = gcd_ll(a.d, b.d), x, y, n, d;

	if (__builtin_mul_overflow(a.n, b.d / g, &x) ||
	    __builtin_mul_overflow(b.n, a.d / g, &y) ||
	    __builtin_add_overflow(x, y, &n) ||
	    __builtin_mul_overflow(a.d / g, b.d, &d))
		return -1;
	g = gcd_ll(n, d);
	r->n = n / g;
	r->d = d / g;
	return 0;
}

/* value in `from`, rewritten in `to`: (v*f_from + off_from - off_to) / f_to. */
static int convert(rat v, const unit_t *from, const unit_t *to, rat *out)
{
	rat f_from = {from->num, from->den}, f_to_inv = {to->den, to->num};
	rat off_from = {from->off_num, from->off_den ? from->off_den : 1};
	rat off_to = {-to->off_num, to->off_den ? to->off_den : 1};
	rat base;

	if (q_mul(&base, v, f_from) < 0 || q_add(&base, base, off_from) < 0 ||
	    q_add(&base, base, off_to) < 0 || q_mul(out, base, f_to_inv) < 0)
		return -1;
	return 0;
}

/*
 * How a converted value is written for the solver.
 *
 * The solvers multiply numerators and denominators in 64 bits without
 * checking, so every digit handed to them is a risk: ten digits of 130 km/h
 * in m/s (36.11111111, numerator 3,611,111,111) squared inside kinetic energy
 * overflows and answers garbage. And the arguments of one call are not
 * independent: 3 lb and 7 lb rounded one at a time to 1.361 and 3.175 kg are
 * no longer 3 to 7, so a collision that cancels to exactly 0 m/s answers
 * 0.0008818. So:
 *
 *  - an exact conversion that six significant digits hold is written exactly:
 *    1.5 km is 1500 m, 27 degC is 300.15 K, 72 km/h is 20 m/s;
 *  - a lone number that has to be rounded keeps the digits the student wrote
 *    plus two, never fewer than four (the answer screen shows four) or more
 *    than six: 130 km/h is 36.111 m/s, which still answers 6520 J;
 *  - two or more numbers written in the same unit, one of which has to be
 *    rounded, all go through that unit pair's factor rounded once to four
 *    digits: 3 lb and 7 lb become 3 x 0.4536 = 1.3608 and 3.1752, still
 *    exactly 3 to 7, so the collision still answers 0. Each product keeps six
 *    digits, so the ratio is exact while products are short and good to about
 *    one part in a million otherwise; writing longer products exactly made
 *    the exact solvers refuse 45% of such groups instead of 33%. Four digits
 *    of factor and not five because five made that very collision overflow.
 *    Celsius and Fahrenheit never share a factor: near absolute zero their
 *    offset cancels most of the product, so each is converted exactly and
 *    rounded on its own, and -459 and -459.6 degF stay 0.37222 and
 *    0.0388889 K rather than 0.3516 and 0.01824.
 *
 * Digits are significant digits, not decimals: a cap on decimals turned
 * 0.0000005 into 0.000001, a different number. The parser takes at most nine
 * decimals, so a value that needs more is not written at all and the caller
 * refuses it rather than pass on a rounded stand-in.
 */
#define RENDER_DIGITS 6
#define RENDER_DECIMALS 9
#define FACTOR_DIGITS 4
#define RENDER_MARGIN 2
#define RENDER_MIN 4
#define PRODUCT_DIGITS 6

/*
 * q as a plain decimal of at most `want` significant digits, trailing zeros
 * dropped. Returns 0 when exact, 1 when rounded, -1 when it cannot be written.
 */
static int render_to(rat q, int want, char *out, size_t outlen)
{
	long long n = q.n < 0 ? -q.n : q.n, d = q.d, ip = n / d, rem = n % d;
	char frac[RENDER_DECIMALS];
	int k = 0, digits = 0, len = 0, rounded, sig;
	long long t;

	if (ip > WIDE_LIMIT)
		return -1;
	if (n == 0)
		return snprintf(out, outlen, "0") < (int)outlen ? 0 : -1;
	for (t = ip; t; t /= 10)
		digits++;
	/* Decimals until `want` significant digits are out or the value is
	 * exact; leading zeros of a value below one are not significant. */
	for (sig = digits; sig < want && rem != 0; k++) {
		if (k == RENDER_DECIMALS)
			return -1;
		rem *= 10;
		frac[k] = (char)('0' + rem / d);
		rem %= d;
		if (sig || frac[k] != '0')
			sig++;
	}
	rounded = rem != 0;
	/* Round half up on the next digit, carrying into the integer part. */
	if (rem * 10 / d >= 5) {
		int i = k - 1;

		while (i >= 0 && frac[i] == '9')
			frac[i--] = '0';
		if (i >= 0)
			frac[i]++;
		else
			ip++;
	}
	while (k > 0 && frac[k - 1] == '0')
		k--;
	len = snprintf(out, outlen, "%s%lld", q.n < 0 && (ip || k) ? "-" : "", ip);
	if (len < 0 || (size_t)len >= outlen)
		return -1;
	if (k) {
		if ((size_t)(len + 1 + k) >= outlen)
			return -1;
		out[len++] = '.';
		memcpy(out + len, frac, (size_t)k);
		out[len + k] = '\0';
	}
	return rounded;
}

/*
 * q as a rational with a short decimal: kept exactly when `exact` digits hold
 * it, rounded to `digits` otherwise.
 */
static int round_rat(rat q, int exact, int digits, rat *out)
{
	char buf[40];

	if (render_to(q, exact, buf, sizeof buf) != 0 &&
	    render_to(q, digits, buf, sizeof buf) < 0)
		return -1;
	return parse_rat(buf, out, NULL) == 0 ? 0 : -1;
}

/* Significant digits as written: "0.0250" has three, "300" three. */
static int written_digits(const char *s)
{
	int n = 0, lead = 1;

	for (; *s && *s != 'e' && *s != 'E'; s++)
		if (is_digit(*s)) {
			if (*s != '0')
				lead = 0;
			if (!lead)
				n++;
		}
	return n;
}

/*
 * `v`, written as `text`, in `from` written in `to` (see "How a converted
 * value is written"). Unless `shared` asks for the common factor, an exact
 * result that six digits hold is written as it is and returns 0, and any
 * other is rounded to the written digits plus two and returns 1. With
 * `shared`, v * factor + offset is written to six digits and returns 1. -1
 * when it cannot be written.
 */
static int convert_value(rat v, const char *text, const unit_t *from,
                         const unit_t *to, int shared, char *out,
                         size_t outlen)
{
	rat zero = {0, 1}, one = {1, 1}, w, at0, at1, factor, offset, scaled;

	if (!shared) {
		int want = written_digits(text) + RENDER_MARGIN;

		want = want < RENDER_MIN ? RENDER_MIN :
		       want > RENDER_DIGITS ? RENDER_DIGITS : want;
		if (convert(v, from, to, &w) < 0)
			return -1;
		if (render_to(w, RENDER_DIGITS, out, outlen) == 0)
			return 0;
		return render_to(w, want, out, outlen);
	}
	/* The pair is affine: to = from * factor + offset. */
	if (convert(zero, from, to, &at0) < 0 || convert(one, from, to, &at1) < 0)
		return -1;
	at0.n = -at0.n;
	if (q_add(&factor, at1, at0) < 0)
		return -1;
	at0.n = -at0.n;
	if (round_rat(factor, FACTOR_DIGITS, FACTOR_DIGITS, &factor) < 0 ||
	    round_rat(at0, RENDER_DIGITS, RENDER_DIGITS, &offset) < 0 ||
	    q_mul(&scaled, v, factor) < 0 || q_add(&w, scaled, offset) < 0 ||
	    render_to(w, PRODUCT_DIGITS, out, outlen) < 0)
		return -1;
	return 1;
}

int quantity_convert(const char *value, const char *from, const char *to,
                     char *out, int outlen)
{
	const unit_t *a = unit_named(from), *b = unit_named(to);
	rat v;

	if (!a || !b || a->kind != b->kind || !a->den || !b->den || outlen <= 0 ||
	    parse_rat(value, &v, NULL) != 0)
		return -1;
	return convert_value(v, value, a, b, 0, out, (size_t)outlen);
}

const char *quantity_unit_alternative(const char *unit, int n)
{
	static char name[24];
	const unit_t *u = unit_named(unit);

	if (!u || !u->den)
		return NULL;
	for (int i = 0; i < N_UNITS; i++)
		if (&UNITS[i] != u && UNITS[i].kind == u->kind && UNITS[i].den &&
		    n-- == 0) {
			canonical(&UNITS[i], name, sizeof name);
			return name;
		}
	return NULL;
}

/* ---- matching arguments to the question ---- */

/* The end of the number that starts at p: digits, a fraction, an exponent. */
static const char *number_end(const char *p)
{
	while (is_digit(*p))
		p++;
	if (*p == '.' && is_digit(p[1]))
		for (p++; is_digit(*p); p++)
			;
	if ((*p == 'e' || *p == 'E') &&
	    (is_digit(p[1]) || ((p[1] == '-' || p[1] == '+') && is_digit(p[2]))))
		for (p += 2; is_digit(*p); p++)
			;
	return p;
}

/*
 * The unit the question wrote after every occurrence of `digits`, or NULL
 * when there is none, it is written twice with different units, or once
 * without one. *found says whether it occurs at all.
 */
static const unit_t *unit_of(const char *digits, const char *question,
                             int *found)
{
	const unit_t *unit = NULL;
	size_t n = strlen(digits);

	*found = 0;
	for (const char *p = question; *p; p++) {
		const char *end;
		const unit_t *u;

		if (!is_digit(*p) || (p > question && (is_digit(p[-1]) || p[-1] == '.')))
			continue;
		end = number_end(p);
		if ((size_t)(end - p) != n || strncmp(p, digits, n) != 0 ||
		    !grounding_number_boundary(question, p, (int)n))
			continue;
		u = unit_after(end, NULL);
		if (!u || (*found && u != unit)) {
			*found = 1;
			return NULL;
		}
		*found = 1;
		unit = u;
	}
	return unit;
}

static const rel_row_t *row_named(const char *name)
{
	for (int i = 0; i < REL_NUM_RELATIONS; i++)
		if (strcmp(rel_rows[i].name, name) == 0)
			return &rel_rows[i];
	return NULL;
}

/* Splits on spaces in place; returns the number of fields, at most max. */
static int split(char *s, char **field, int max)
{
	int n = 0;

	while (*s && n < max) {
		while (*s == ' ')
			*s++ = '\0';
		if (!*s)
			break;
		field[n++] = s;
		while (*s && *s != ' ')
			s++;
	}
	return n;
}

#define MAX_FIELDS 12

/*
 * The unit a slot asks for. A slot may list alternatives ("M|mol"): a ratio
 * slot like a buffer's base and acid computes the same answer from moles in
 * one solution as from their molarities. The alternative of the kind the
 * number was written in is the one it converts to; with no number, or no
 * alternative of its kind, the first is.
 */
static const unit_t *slot_choice(const char *slot, const unit_t *have)
{
	const unit_t *first = NULL, *u;
	char name[24];

	for (const char *p = slot; *p;) {
		size_t n = strcspn(p, "|");

		snprintf(name, sizeof name, "%.*s", (int)n, p);
		if ((u = unit_named(name)) != NULL) {
			if (!first)
				first = u;
			if (have && u->kind == have->kind)
				return u;
		}
		p += n;
		if (*p == '|')
			p++;
	}
	return first;
}

/* Celsius and Fahrenheit carry an offset; every other conversion scales. */
static int affine(const unit_t *a, const unit_t *b)
{
	return a->off_num != 0 || b->off_num != 0;
}

int quantity_check_call(const char *call, const char *question,
                        char *out, int outlen)
{
	char work[256], units[128], value[8][32];
	char *f[MAX_FIELDS], *slot[MAX_FIELDS];
	const char *arg[8];
	const unit_t *want[8], *have[8];
	const rel_row_t *row;
	rat v[8];
	int todo[8], shared[8], inexact[8], unscaled_zero[8];
	int nf, ns, nargs, converted = 0, len;

	if (!out || outlen <= 0)
		return QUANTITY_SAME;
	snprintf(out, (size_t)outlen, "%s", call ? call : "");
	if (!call || !question || strlen(call) >= sizeof work)
		return QUANTITY_SAME;
	memcpy(work, call, strlen(call) + 1);
	nf = split(work, f, MAX_FIELDS);
	if (nf < 3 || strcmp(f[0], "CALL") != 0 || !(row = row_named(f[1])))
		return QUANTITY_SAME;
	nargs = nf - 2;
	snprintf(units, sizeof units, "%s", row->units);
	ns = split(units, slot, MAX_FIELDS);
	if (nargs != row->arity || ns != nargs || nargs > 8)
		return QUANTITY_SAME;

	for (int i = 0; i < nargs; i++) {
		const char *digits = f[2 + i];
		int found;

		arg[i] = f[2 + i];
		want[i] = have[i] = NULL;
		todo[i] = 0;
		unscaled_zero[i] = 0;
		if (strcmp(slot[i], "-") == 0 || !(want[i] = slot_choice(slot[i], NULL)))
			continue;
		if (*digits == '-' || *digits == '+')
			digits++;
		have[i] = unit_of(digits, question, &found);
		if (!have[i]) {
			/* Zero heat, work or speed stays zero under scaling. Absolute
			 * temperatures can carry an offset, so keep their fallback. */
			unscaled_zero[i] = want[i]->kind != K_TEMP &&
			                   parse_rat(arg[i], &v[i], NULL) == 0 && v[i].n == 0;
			continue;
		}
		want[i] = slot_choice(slot[i], have[i]);
		if (have[i] == want[i])
			continue;
		if (have[i]->kind != want[i]->kind || !have[i]->den || !want[i]->den) {
			/* 0 and 1 are the values grounding accepts with no number in
			 * the text ("one mole in 1 kg"), so the unit after some other
			 * 1 is no evidence about this one. */
			if (strcmp(digits, "0") == 0 || strcmp(digits, "1") == 0)
				continue;
			return QUANTITY_MISMATCH;
		}
		if (parse_rat(f[2 + i], &v[i], NULL) != 0)
			return QUANTITY_MISMATCH;
		todo[i] = 1;
	}

	/*
	 * Slots that offer the same alternatives are the two sides of one ratio,
	 * so they must be written in the same kind: 2 mol of base over 3 mol of
	 * acid is a ratio, 2 mol over 3 M is not.
	 */
	for (int i = 0; i < nargs; i++)
		for (int j = i + 1; j < nargs; j++)
			if (have[i] && have[j] && strchr(slot[i], '|') &&
			    strcmp(slot[i], slot[j]) == 0 &&
			    have[i]->kind != have[j]->kind)
				return QUANTITY_MISMATCH;

	/*
	 * A unit can be shared: "2 and 8 kg" writes kg once, for both. When an
	 * argument of the same kind as one to be converted carries no unit of its
	 * own, it may be in the unit being converted from, and converting only
	 * its partner would turn 25% into 0.025%. Leave the group as decoded. A
	 * partner in the same alternative slot counts too, whatever kind its
	 * first alternative is.
	 */
	for (int i = 0; i < nargs; i++)
		for (int j = 0; todo[i] && j < nargs; j++)
			if (j != i && want[j] &&
			    (want[j]->kind == want[i]->kind ||
			     strcmp(slot[j], slot[i]) == 0) &&
			    !have[j] && !unscaled_zero[j])
				todo[i] = 0;

	/* Numbers in one unit, any of which has to be rounded, share the
	 * rounded factor so their ratios survive (see "How a converted value is
	 * written"); a number alone in its unit is rounded on its own. A
	 * temperature never shares: its offset cancels most of the product near
	 * absolute zero, so a factor error of 0.01% becomes 5% of the answer, and
	 * ratios of Fahrenheit readings mean nothing to preserve. Each is
	 * converted exactly and then rounded. */
	for (int i = 0; i < nargs; i++)
		inexact[i] = todo[i] &&
		             convert_value(v[i], f[2 + i], have[i], want[i], 0, value[i],
		                           sizeof value[i]) != 0;
	for (int i = 0; i < nargs; i++) {
		shared[i] = 0;
		for (int j = 0; todo[i] && j < nargs; j++)
			if (j != i && todo[j] && have[j] == have[i] &&
			    want[j] == want[i] && (inexact[i] || inexact[j]) &&
			    !affine(have[i], want[i]))
				shared[i] = 1;
	}

	for (int i = 0; i < nargs; i++) {
		if (!todo[i])
			continue;
		/* A value this cannot hold or print is refused, never passed on in
		 * the wrong unit. */
		if ((shared[i] || inexact[i]) &&
		    convert_value(v[i], f[2 + i], have[i], want[i], shared[i],
		                  value[i], sizeof value[i]) < 0)
			return QUANTITY_MISMATCH;
		arg[i] = value[i];
		converted = 1;
	}
	if (!converted)
		return QUANTITY_SAME;

	len = snprintf(out, (size_t)outlen, "CALL %s", f[1]);
	for (int i = 0; i < nargs && len >= 0 && len < outlen; i++)
		len += snprintf(out + len, (size_t)(outlen - len), " %s", arg[i]);
	if (len < 0 || len >= outlen) {
		snprintf(out, (size_t)outlen, "%s", call);
		return QUANTITY_MISMATCH;
	}
	return QUANTITY_CONVERTED;
}
