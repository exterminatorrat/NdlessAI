/*
 * The unit reader: what it reads after a number, and that every unit a slot
 * in reltable.h declares is one it can read.
 *
 *     ./test_quantity
 */
#include <stdio.h>
#include <string.h>

#include "quantity.h"
#include "reltable.h"

static int failures;

static void unit_is(const char *text, const char *want)
{
	const char *got = quantity_unit_after(text, NULL);

	if (want ? !got || strcmp(got, want) != 0 : got != NULL) {
		printf("FAIL unit after \"%s\": %s, wanted %s\n", text,
		       got ? got : "none", want ? want : "none");
		failures++;
	}
}

static void fixed_cases(void)
{
	/* Reading a unit: the longest spelling that ends on a word boundary. */
	unit_is(" m/s^2 later", "m/s^2");
	unit_is(" m/s, then", "m/s");
	unit_is(" km/h.", "km/h");
	unit_is("-kg block", "kg");
	unit_is(" kilograms", "kg");
	unit_is(" degrees C", "degC");
	unit_is("\xc2\xb0" "C", "degC");
	unit_is(" degrees Celsius", "degC");
	unit_is(" degrees, clockwise", "degrees");
	unit_is(" mol/L", "M");
	unit_is(" kJ per mole", "kJ/mol");
	unit_is(" in 3 s", NULL);          /* a preposition, not inches */
	unit_is(" mph", "mph");
	unit_is(" meters long", "m");
	unit_is(" metres per second, then", "m/s");
	unit_is(" metres per second squared", "m/s^2");
	unit_is(" newtons per metre", "N/m");
	unit_is(" metres per minute", NULL);  /* a compound it cannot spell */
	unit_is(" mass", NULL);            /* "m" must not match a word */
	unit_is(" J/(g C)", NULL);         /* a compound this does not read */
	/* A compound is read whole or not at all, never as its first part. */
	unit_is(" m/s\xc2\xb2 down", "m/s^2");
	unit_is(" m\xc2\xb2", "m^2");
	unit_is(" m / s^2", NULL);
	unit_is(" m s^-1", NULL);
	unit_is(" m s\xe2\x81\xbb\xc2\xb9", NULL);
	unit_is(" m/s\xe2\x81\xbb", NULL);

	/* Every slot unit reltable.h declares is one this module can read. */
	for (int i = 0; i < REL_NUM_RELATIONS; i++) {
		char units[128], *tok;

		snprintf(units, sizeof units, "%s", rel_rows[i].units);
		for (tok = strtok(units, " |"); tok; tok = strtok(NULL, " |"))
			if (strcmp(tok, "-") != 0 && !quantity_unit_known(tok)) {
				printf("FAIL %s declares unknown unit %s\n",
				       rel_rows[i].name, tok);
				failures++;
			}
	}
}

int main(void)
{
	fixed_cases();
	if (failures) {
		printf("%d failures\n", failures);
		return 1;
	}
	printf("all checks passed\n");
	return 0;
}
