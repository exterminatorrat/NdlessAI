#include <stdio.h>
#include <string.h>

#include "quantity.h"

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
