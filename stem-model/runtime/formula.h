#ifndef FORMULA_H
#define FORMULA_H

#include <stddef.h>

#include "chemtable.h"

/*
 * A chemical formula, read exactly.
 *
 * The parser is the reference implementation in `chem/formula.py` written a
 * second time in the C the calculator runs, over one generated table
 * (`chemtable.h`), so the two cannot disagree about an atomic weight or a
 * compound's formula. It refuses rather than guessing: an element symbol that
 * does not exist, an unbalanced or empty group, a count of zero, a formula
 * past what this build holds, and a name that is not in the measured table all
 * produce a negative return and no answer.
 *
 * Every entry point returns 0 on success and a negative code otherwise:
 *   -1 not a formula or an unknown element or name
 *   -2 unbalanced parenthesis
 *   -3 malformed text
 *   -4 a count, size or charge outside what this build holds
 */

#define FORMULA_MAX_TEXT 64

/* Element counts indexed as chem_elements. `counts` must have room for
 * CHEM_NUM_ELEMENTS entries. */
int formula_composition(const char *text, unsigned short *counts);

/* Molar mass in integer thousandths of a gram per mole: CH3OH is 32042. */
int formula_molar_mass_mg(const char *text, long long *mg);

/* 0 when the text is a formula or one of the measured names, negative when it
 * is neither. Used by the typed-call parser to refuse an argument before the
 * solver runs, rather than after. */
int formula_recognise(const char *text);

/* Cations and anions supplied by one formula unit of an ionic compound. */
int formula_ion_counts(const char *text, int *cations, int *anions);

/*
 * The formula-taking calls, dispatched from solvers_solve beside the numeric
 * relations. They take a formula or a measured compound name where the
 * catalogue takes a number, and they are reached by typing the call
 * (`molar_mass_formula CH3OH`), so they need no model and no retrain.
 *
 * Same return convention as the other subject solvers: 0 answered, -1 not one
 * of these names, -2 wrong number of arguments, -3 an argument it cannot read.
 */
int formula_solve(const char *name, const char *const *args, int nargs,
                  char *out, size_t outlen);

/*
 * The working for those calls. The derived quantity that the catalogue would
 * have demanded as an argument is printed as its own line, so a molar mass the
 * app computed is visible rather than hidden in an answer.
 *
 * Returns 1 when a block was written, 0 when the call has no such block, and
 * -1 when the block did not fit (still NUL-terminated).
 */
int formula_steps(const char *name, const char *const *args, int nargs,
                  const char *answer, char *out, int outlen);

#endif
