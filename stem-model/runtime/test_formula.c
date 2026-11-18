/*
 * The formula parser, against the formulas a chemistry student writes.
 *
 * The refusals matter as much as the answers: a molar mass computed from a
 * misread formula is a wrong number indistinguishable from a right one. Every
 * case below that expects a negative return is a case the app must decline.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "formula.h"
#include "solvers.h"
#include "steps.h"

static int failures;

static void check(int cond, const char *what)
{
	if (!cond) {
		printf("FAIL %s\n", what);
		failures++;
	}
}

static int element(const char *symbol)
{
	int i;

	for (i = 0; i < CHEM_NUM_ELEMENTS; i++)
		if (strcmp(chem_elements[i].symbol, symbol) == 0)
			return i;
	return -1;
}

static int count_of(const char *formula, const char *symbol)
{
	unsigned short counts[CHEM_NUM_ELEMENTS];
	int idx = element(symbol);

	if (formula_composition(formula, counts) != 0 || idx < 0)
		return -1;
	return counts[idx];
}

static long long mass_of(const char *formula)
{
	long long mg = 0;

	if (formula_molar_mass_mg(formula, &mg) != 0)
		return -1;
	return mg;
}

static int ion_counts(const char *formula, int *cations, int *anions)
{
	return formula_ion_counts(formula, cations, anions);
}

static void test_composition(void)
{
	check(count_of("H2O", "H") == 2 && count_of("H2O", "O") == 1,
	      "water is H2O");
	check(count_of("Ba(NO3)2", "Ba") == 1 &&
	      count_of("Ba(NO3)2", "N") == 2 &&
	      count_of("Ba(NO3)2", "O") == 6,
	      "a nested group multiplies its subscript");
	check(count_of("Al2(SO4)3", "Al") == 2 &&
	      count_of("Al2(SO4)3", "S") == 3 &&
	      count_of("Al2(SO4)3", "O") == 12,
	      "a group after a symbol multiplies both ways");
	/* Unicode subscripts, written as their UTF-8 bytes: H₂O and Ba(NO₃)₂. */
	check(count_of("H\xE2\x82\x82O", "H") == 2, "a unicode subscript is a digit");
	check(count_of("Ba(NO\xE2\x82\x83)\xE2\x82\x82", "O") == 6,
	      "unicode subscripts inside and after a group");
	/* The middle dot of a hydrate, C2 B7. */
	check(count_of("CuSO4\xC2\xB7" "5H2O", "O") == 9 &&
	      count_of("CuSO4\xC2\xB7" "5H2O", "H") == 10,
	      "a hydrate multiplier applies to its own part only");
}

static void test_molar_mass(void)
{
	check(mass_of("H2O") == 18015, "the molar mass of water");
	check(mass_of("CH3OH") == 32042, "the molar mass of methanol");
	check(mass_of("NaCl") == 58440, "the molar mass of sodium chloride");
	check(mass_of("CaCO3") == 100086, "the molar mass of calcium carbonate");
	check(mass_of("C6H12O6") == 180156, "the molar mass of glucose");
	check(mass_of("CuSO4\xC2\xB7" "5H2O") == 249677,
	      "the molar mass of a hydrate");
	check(mass_of("methanol") == mass_of("CH3OH"),
	      "a measured name is its formula");
	check(mass_of("water") == mass_of("H2O"), "water is H2O");
	check(mass_of("carbon_dioxide") == mass_of("CO2"),
	      "an underscore stands in for the space in a name");
	check(mass_of("acetic-acid") == mass_of("CH3COOH"),
	      "a hyphen does the same");
	check(mass_of("Ba2+") == mass_of("Ba"), "a charge does not change a mass");
	check(mass_of("Ba(NO3)2 2+") == mass_of("Ba(NO3)2"),
	      "a charge written as its own token is read, not multiplied in");
}

static void test_refusals(void)
{
	long long mg;
	int c, a;

	check(formula_molar_mass_mg("H2X", &mg) < 0, "an unknown symbol refuses");
	check(formula_molar_mass_mg("Xx2", &mg) < 0, "a non-element refuses");
	check(formula_molar_mass_mg("Ca(OH2", &mg) < 0, "unbalanced parens refuse");
	check(formula_molar_mass_mg("CaOH)2", &mg) < 0, "a stray close refuses");
	check(formula_molar_mass_mg("H0", &mg) < 0, "a subscript of zero refuses");
	check(formula_molar_mass_mg("2H2O", &mg) < 0,
	      "a coefficient is not a formula");
	check(formula_molar_mass_mg("propanol", &mg) < 0,
	      "an isomer the formula cannot settle refuses");
	check(formula_molar_mass_mg("unobtainium", &mg) < 0,
	      "an unknown name refuses");
	check(formula_molar_mass_mg("", &mg) < 0, "empty text refuses");
	check(formula_ion_counts("Na2O2", &c, &a) < 0,
	      "peroxide does not balance as written");
}

static void test_ions(void)
{
	int c, a;

	c = a = 0;
	check(ion_counts("NaCl", &c, &a) == 0 && c == 1 && a == 1, "NaCl");
	check(ion_counts("MgCl2", &c, &a) == 0 && c == 1 && a == 2, "MgCl2");
	check(ion_counts("Ca(OH)2", &c, &a) == 0 && c == 1 && a == 2, "Ca(OH)2");
	check(ion_counts("Ba(NO3)2", &c, &a) == 0 && c == 1 && a == 2,
	      "Ba(NO3)2");
	check(ion_counts("K2CO3", &c, &a) == 0 && c == 2 && a == 1, "K2CO3");
	check(ion_counts("Al2(SO4)3", &c, &a) == 0 && c == 2 && a == 3,
	      "Al2(SO4)3");
	check(ion_counts("Cu3(PO4)2", &c, &a) == 0 && c == 3 && a == 2,
	      "Cu3(PO4)2 settles copper's charge");
	check(ion_counts("KMnO4", &c, &a) == 0 && c == 1 && a == 1, "KMnO4");
	check(ion_counts("NaC2H3O2", &c, &a) == 0 && c == 1 && a == 1,
	      "acetate");
	check(ion_counts("NH4Cl", &c, &a) == 0 && c == 1 && a == 1, "NH4Cl");
	check(ion_counts("(NH4)2SO4", &c, &a) == 0 && c == 2 && a == 1,
	      "(NH4)2SO4");
	check(ion_counts("Fe(NO3)3", &c, &a) == 0 && c == 1 && a == 3,
	      "Fe(NO3)3 settles iron's charge");
	check(ion_counts("CaF2", &c, &a) == 0 && c == 1 && a == 2, "CaF2");
	check(ion_counts("CH4", &c, &a) < 0, "a molecular formula has no ions");
	check(ion_counts("CO2", &c, &a) < 0, "carbon dioxide has no ions");
	check(ion_counts("ClF3", &c, &a) < 0, "an interhalogen has no ions");
	check(ion_counts("NaCl2", &c, &a) < 0, "unbalanced charges refuse");
	check(ion_counts("CuSO4\xC2\xB7" "5H2O", &c, &a) < 0,
	      "a hydrate is not one formula unit");
	check(ion_counts("Ba2+", &c, &a) < 0, "an ion is not a formula unit");
	check(ion_counts("Na", &c, &a) < 0, "an element is not a formula unit");
}

static void test_calls(void)
{
	const char *one[1] = { "CH3OH" };
	const char *two[2] = { "128", "CH3OH" };
	const char *numbers[2] = { "128", "32" };
	char out[256];

	check(solvers_solve("molar_mass_formula", one, 1, out, sizeof out) == 0 &&
	      strcmp(out, "32.04 g/mol") == 0,
	      "molar_mass_formula answers CH3OH");
	check(solvers_solve("molar_mass_formula", one, 2, out, sizeof out) != 0,
	      "the wrong number of arguments is refused");
	check(solvers_solve("molar_mass_formula", two, 1, out, sizeof out) != 0 &&
	      out[0] == '\0', "an unknown formula refuses with no answer");
	check(solvers_solve("moles", numbers, 2, out, sizeof out) == 0 &&
	      strcmp(out, "4 mol") == 0,
	      "the numeric relations still answer");

	check(steps_render("molar_mass_formula", one, 1, "32.04 g/mol",
	                   out, (int)sizeof out) == 1 &&
	      strstr(out, "M(CH3OH)") != NULL &&
	      strstr(out, "= 32.04 g/mol") != NULL,
	      "the working shows the molar mass it derived");
	check(steps_render("moles", numbers, 2, "4 mol", out, (int)sizeof out) == 1 &&
	      strstr(out, "n = m/M") != NULL,
	      "the numeric relation keeps its own working");
}

/*
 * The fixture holds what the Python reference answered for the same formulas,
 * so this walks it and compares row by row. A missing file is a failure rather
 * than a skip: the host test is where the two implementations are compared.
 */
static void test_reference_fixture(const char *path)
{
	FILE *f = fopen(path, "r");
	char line[256];
	char text[128] = "";

	if (f == NULL) {
		printf("FAIL cannot open %s\n", path);
		failures++;
		return;
	}
	while (fgets(line, (int)sizeof line, f)) {
		char *nl = strchr(line, '\n');

		if (nl)
			*nl = '\0';
		if (line[0] == '#' || line[0] == '\0')
			continue;
		if (strncmp(line, "TEXT\t", 5) == 0) {
			snprintf(text, sizeof text, "%s", line + 5);
		} else if (strncmp(line, "MASS\t", 5) == 0) {
			long long want, mg = 0;
			int rc = formula_molar_mass_mg(text, &mg);

			if (strcmp(line + 5, "-") == 0) {
				check(rc < 0, text);
			} else {
				want = atoll(line + 5);
				check(rc == 0 && mg == want, text);
			}
		} else if (strncmp(line, "IONS\t", 5) == 0 && strcmp(line + 5, "-\t-") == 0) {
			int c, a;

			check(formula_ion_counts(text, &c, &a) < 0, text);
		} else if (strncmp(line, "IONS\t", 5) == 0) {
			int want_c = atoi(line + 5);
			const char *tab = strchr(line + 5, '\t');
			int c, a;

			check(tab != NULL, text);
			if (tab == NULL)
				continue;
			check(formula_ion_counts(text, &c, &a) == 0 &&
			      c == want_c && a == atoi(tab + 1), text);
		}
	}
	fclose(f);
}

int main(int argc, char **argv)
{
	test_composition();
	test_molar_mass();
	test_refusals();
	test_ions();
	test_calls();
	test_reference_fixture(argc > 1 ? argv[1] : "fixtures/chem_cases.txt");
	if (failures) {
		printf("formula: %d failure(s)\n", failures);
		return 1;
	}
	printf("formula: all cases passed\n");
	return 0;
}
