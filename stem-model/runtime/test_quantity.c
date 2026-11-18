/*
 * The unit check: fixed cases for each outcome, then, with --cases, every
 * correct call gen_unit_cases.py writes from the generators and the
 * hand-written corpus. Three passes over that corpus:
 *
 *  - a correct call must come back unchanged, so a wrong slot unit in
 *    param_units.py or a misread unit in quantity.c fails here instead of
 *    refusing a student's right answer;
 *  - every tagged number is rewritten into each other unit of its kind, and
 *    the converted call must reach the solver and answer what the original
 *    did, so a conversion that is right as text but overflows or misleads the
 *    solver fails too;
 *  - swapped arguments are counted, not gated: how many the units now catch.
 *
 *     ./test_quantity
 *     python3 ../gen_unit_cases.py | ./test_quantity --cases
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "grounding.h"
#include "quantity.h"
#include "reltable.h"
#include "solvers.h"

/* Revised 2026-11-17: verify unit conversions beside implicit zero values. */

static int failures;

static void expect(const char *question, const char *call, int want_rc,
                   const char *want_call)
{
	char out[256];
	int rc = quantity_check_call(call, question, out, (int)sizeof out);

	if (rc != want_rc || strcmp(out, want_call) != 0) {
		printf("FAIL %s | %s: rc %d \"%s\", wanted %d \"%s\"\n", question,
		       call, rc, out, want_rc, want_call);
		failures++;
	}
}

static void same(const char *question, const char *call)
{
	expect(question, call, QUANTITY_SAME, call);
}

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

	/* Same unit: left exactly as decoded. */
	same("A 10 kg ball moves at 20 m/s. Find its kinetic energy.",
	     "CALL kinetic_energy 10 20");
	same("88 g of CO2 at 44 g/mol: how many moles?", "CALL moles 88 44");
	same("Burning 2 mol releases -890 kJ per mole. Find the heat.",
	     "CALL heat_reaction 2 -890");

	/* Same kind: converted exactly, in the slot's unit. */
	expect("A 10 kg ball moves at 72 km/h. Find its kinetic energy.",
	       "CALL kinetic_energy 10 72", QUANTITY_CONVERTED,
	       "CALL kinetic_energy 10 20");
	expect("A 10 kg ball moves at 130 km/h. Find its kinetic energy.",
	       "CALL kinetic_energy 10 130", QUANTITY_CONVERTED,
	       "CALL kinetic_energy 10 36.111");
	/* Six significant digits, not six decimals: a cap on decimals made
	 * this 0.000002, a third too heavy. */
	expect("A 1.5 mg grain moves at 20 m/s. Find its kinetic energy.",
	       "CALL kinetic_energy 1.5 20", QUANTITY_CONVERTED,
	       "CALL kinetic_energy 0.0000015 20");
	expect("A 500 g ball moves at 20 m/s. Find its kinetic energy.",
	       "CALL kinetic_energy 500 20", QUANTITY_CONVERTED,
	       "CALL kinetic_energy 0.5 20");
	expect("A machine does 600 J of work in 2 minutes. Find its power.",
	       "CALL power 600 2", QUANTITY_CONVERTED, "CALL power 600 120");
	expect("0.5 mol is dissolved in 250 mL. Find the molarity.",
	       "CALL molarity 0.5 250", QUANTITY_CONVERTED,
	       "CALL molarity 0.5 0.25");
	expect("A 2 L balloon at 27 degrees C is heated to 127 degrees C.",
	       "CALL charles 2 27 127", QUANTITY_CONVERTED,
	       "CALL charles 2 300.15 400.15");
	expect("A 2 L balloon at -40 degrees F is warmed to 300 K.",
	       "CALL charles 2 -40 300", QUANTITY_CONVERTED,
	       "CALL charles 2 233.15 300");

	/* Another kind, or no exact rate: refused, the call left as decoded. */
	expect("A 10 kg ball moves at 72 km/h. Find its kinetic energy.",
	       "CALL kinetic_energy 72 10", QUANTITY_MISMATCH,
	       "CALL kinetic_energy 72 10");
	expect("A 2 kg cart and a 3 kg cart. Find the momentum.",
	       "CALL momentum 2 3", QUANTITY_MISMATCH, "CALL momentum 2 3");
	expect("A 2-kg block is pushed with 3 kN for 4 s. Find the impulse.",
	       "CALL impulse 2 4", QUANTITY_MISMATCH, "CALL impulse 2 4");
	expect("Find sin of 0.5 rad.", "CALL sin_eval 0.5", QUANTITY_MISMATCH,
	       "CALL sin_eval 0.5");

	/* A converted call must still be one the solver can compute: ten digits
	 * of 130 km/h squared overflowed kinetic energy's 64-bit product. Two
	 * more digits than "130" was written with still shows 6520 J. */
	{
		const char *args[] = {"10", "36.111"};
		char answer[64];

		if (solvers_solve("kinetic_energy", args, 2, answer, sizeof answer) != 0 ||
		    strcmp(answer, "6520 J") != 0) {
			printf("FAIL 10 kg at 130 km/h answers \"%s\"\n", answer);
			failures++;
		}
	}

	/* A unit written once for two numbers is shared: converting only the
	 * tagged one would turn 25% into 0.025%. Written on each, it converts. */
	same("The solute and solution masses are 2 and 8 kg.",
	     "CALL mass_percent 2 8");
	expect("The solute is 2 kg and the solution 8 kg.", "CALL mass_percent 2 8",
	       QUANTITY_CONVERTED, "CALL mass_percent 2000 8000");

	/* Numbers in one unit share one rounded factor, so 3 lb and 7 lb stay
	 * exactly 3 to 7 and a collision that cancels still answers 0. */
	{
		const char *q = "A 3 lb cart at 14 m/s hits a 7 lb cart at -6 m/s and "
		                "they stick. Find the final velocity.";
		char out[256], answer[64], work[256], *g[8];
		int n = 0;

		expect(q, "CALL collision_velocity 3 14 7 -6", QUANTITY_CONVERTED,
		       "CALL collision_velocity 1.3608 14 3.1752 -6");
		quantity_check_call("CALL collision_velocity 3 14 7 -6", q, out,
		                    (int)sizeof out);
		snprintf(work, sizeof work, "%s", out);
		for (char *t = strtok(work, " "); t && n < 8; t = strtok(NULL, " "))
			g[n++] = t;
		if (n != 6 || solvers_solve(g[1], (const char *const *)g + 2, 4, answer,
		                            sizeof answer) != 0 ||
		    strcmp(answer, "0 m/s") != 0) {
			printf("FAIL the lb collision answers \"%s\", wanted \"0 m/s\"\n",
			       n == 6 ? answer : "?");
			failures++;
		}
	}
	/* An exact conversion and a rounded one in the same unit share the
	 * factor too, so 72 and 130 km/h keep their ratio. */
	expect("Cart one does 72 km/h and cart two 130 km/h; both are 10 kg.",
	       "CALL collision_velocity 10 72 10 130", QUANTITY_CONVERTED,
	       "CALL collision_velocity 10 20.0016 10 36.114");
	/* Exact conversions in one unit need no factor: each stays exact. */
	expect("Cart one does 72 km/h and cart two 36 km/h; both are 10 kg.",
	       "CALL collision_velocity 10 72 10 36", QUANTITY_CONVERTED,
	       "CALL collision_velocity 10 20 10 10");

	/* Temperatures never share a rounded factor: near absolute zero the
	 * offset cancels most of the product, and a shared factor made these
	 * 0.3516 and 0.01824 K, a Carnot efficiency of 0.9481 for 0.8955. The
	 * exact solver refuses five-digit decimals here, which is its own limit
	 * ("carnot 0.372222 0.038889" fails typed directly), so what is checked
	 * is that the values are right and the answer is never the wrong one:
	 * refused, or 0.8955, which the same values give at four digits. */
	{
		const char *q = "Two reservoirs sit at -459 degrees F and -459.6 "
		                "degrees F. Find the Carnot efficiency.";
		const char *args[] = {"0.37222", "0.0388889"};
		const char *four[] = {"0.3722", "0.03889"};
		char answer[64];

		expect(q, "CALL carnot -459 -459.6", QUANTITY_CONVERTED,
		       "CALL carnot 0.37222 0.0388889");
		if (solvers_solve("carnot", args, 2, answer, sizeof answer) == 0 &&
		    strcmp(answer, "0.8955") != 0) {
			printf("FAIL the -459 degF reservoirs answer \"%s\"\n", answer);
			failures++;
		}
		if (solvers_solve("carnot", four, 2, answer, sizeof answer) != 0 ||
		    strcmp(answer, "0.8955") != 0) {
			printf("FAIL the reservoirs at four digits answer \"%s\"\n", answer);
			failures++;
		}
	}

	/* A ratio slot may take either kind its alternatives offer, as long as
	 * both sides use the same one: moles of base and acid in one solution
	 * give the pH their molarities do. */
	same("A buffer holds 2 mol of conjugate base and 3 mol of acid, pKa 4.76.",
	     "CALL buffer_ph 4.76 2 3");
	same("A buffer is 0.2 M in base and 0.3 M in acid, pKa 4.76.",
	     "CALL buffer_ph 4.76 0.2 0.3");
	expect("A buffer holds 2 mmol of base and 0.003 mol of acid, pKa 4.76.",
	       "CALL buffer_ph 4.76 2 0.003", QUANTITY_CONVERTED,
	       "CALL buffer_ph 4.76 0.002 0.003");
	expect("A buffer is 0.2 M in base and 300 mM in acid, pKa 4.76.",
	       "CALL buffer_ph 4.76 0.2 300", QUANTITY_CONVERTED,
	       "CALL buffer_ph 4.76 0.2 0.3");
	expect("A buffer holds 2 mol of base and is 3 M in acid, pKa 4.76.",
	       "CALL buffer_ph 4.76 2 3", QUANTITY_MISMATCH,
	       "CALL buffer_ph 4.76 2 3");
	same("The reaction gave 0.5 mol of product against a theoretical 0.8 mol.",
	     "CALL percent_yield 0.5 0.8");
	expect("The reaction gave 450 g of product against a theoretical 0.5 kg.",
	       "CALL percent_yield 450 0.5", QUANTITY_CONVERTED,
	       "CALL percent_yield 450 500");
	expect("The reaction gave 0.5 mol of product against a theoretical 80 g.",
	       "CALL percent_yield 0.5 80", QUANTITY_MISMATCH,
	       "CALL percent_yield 0.5 80");
	/* An implicit partner (the 1 is not written) may share the unit, so the
	 * pair is left as decoded. */
	same("It gave 0.5 kg of product.", "CALL percent_yield 0.5 1");

	/* Anything unreadable is left alone. */
	same("A 10 kg cart moves at 10 m/s.", "CALL kinetic_energy 10 10");
	same("A mass of 10 moves at a speed of 72.", "CALL kinetic_energy 10 72");
	same("Find the speed of the 10 kg cart.", "CALL kinetic_energy 10 1");
	same("mu is 0.4 and the normal force 500 N.", "CALL friction_force 0.4 500");
	same("A 10 kg ball at 72 km/h.", "CALL no_such_relation 10 72");
	same("A 10 kg ball at 72 km/h.", "CALL kinetic_energy 10");
	same("A 10 kg ball at 72 km/h.", "not a call");
	same("A rise of 40 degrees in 20 g.", "CALL specific_heat 20 4.18 40");
	/* Deliberately undeclared: the corpus writes this acceleration as a
	 * speed reached in one second (see param_units.py). */
	same("A 2 kg puck reaches 3 m/s in one second.", "CALL newton2 2 3");
	/* 0 and 1 may be implicit, so a unit after another 1 says nothing. */
	expect("One mole in 1 lb of solvent: find the molality.",
	       "CALL molality 1 1", QUANTITY_CONVERTED, "CALL molality 1 0.4536");
	/* Read whole or not at all: neither is a length or a speed. */
	same("A ball falls 20 m with g = 9.8 m/s\xc2\xb2.", "CALL free_fall 20 9.8");
	same("A ball falls 20 m with g = 9.8 m / s^2.", "CALL free_fall 20 9.8");
	/* Scale-free, or a rate whose time basis is not a slot: undeclared. */
	same("A sample of 80 g has a half life of 2 days. How much remains after "
	     "4 days?", "CALL half_life 80 2 4");
	same("1000 dollars earns 5% per day for 2 days. Find the simple interest.",
	     "CALL simple_interest 1000 5 2");
}

static void zero_cases(void)
{
	static const struct {
		const char *question, *call, *converted, *answer;
	} cases[] = {
		{"A system absorbs 5 kJ of heat. No work is done.",
		 "CALL first_law 5 0", "CALL first_law 5000 0", "5000 J"},
		{"A system absorbs no heat and receives 5 kJ of work.",
		 "CALL first_law 0 5", "CALL first_law 0 5000", "5000 J"},
		{"A 10 kg cart accelerates from rest to 72 km/h.",
		 "CALL work_energy 10 0 72", "CALL work_energy 10 0 20", "2000 J"},
		{"A 10 kg cart slows from 72 km/h to rest.",
		 "CALL work_energy 10 72 0", "CALL work_energy 10 20 0", "-2000 J"}
	};

	for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) {
		char out[256], answer[128], work[256], *rel, *t;
		const char *args[8];
		int n = 0;

		expect(cases[i].question, cases[i].call, QUANTITY_CONVERTED,
		       cases[i].converted);
		quantity_check_call(cases[i].call, cases[i].question, out, sizeof out);
		snprintf(work, sizeof work, "%s", out);
		strtok(work, " ");
		rel = strtok(NULL, " ");
		while ((t = strtok(NULL, " ")) && n < 8)
			args[n++] = t;
		if (!rel || solvers_solve(rel, args, n, answer, sizeof answer) != 0 ||
		    strcmp(answer, cases[i].answer) != 0) {
			printf("FAIL zero case: %s; wanted %s\n", cases[i].question,
			       cases[i].answer);
			failures++;
		}
	}
	expect("A system absorbs 5 kJ with no work done.", "CALL first_law 5 -0",
	       QUANTITY_CONVERTED, "CALL first_law 5000 -0");
	expect("A system absorbs 5 kJ with no work done.", "CALL first_law 5 0.0",
	       QUANTITY_CONVERTED, "CALL first_law 5000 0.0");
	same("Heat and work are 2 and 8 kJ.", "CALL first_law 2 8");
	same("A 2 L balloon is heated to 27 degC.", "CALL charles 2 0 27");
	expect("A 2 L balloon at 0 degC is heated to 27 degC.",
	       "CALL charles 2 0 27", QUANTITY_CONVERTED,
	       "CALL charles 2 273.15 300.15");
}

/* ---- the corpus ---- */

static int split_args(char *call, char **field, int max)
{
	int n = 0;

	for (char *t = strtok(call, " "); t && n < max; t = strtok(NULL, " "))
		field[n++] = t;
	return n;
}

static const rel_row_t *row_named(const char *name)
{
	for (int i = 0; i < REL_NUM_RELATIONS; i++)
		if (strcmp(rel_rows[i].name, name) == 0)
			return &rel_rows[i];
	return NULL;
}

/* The `i`-th space-separated unit of a row, into out. */
static void slot_unit(const rel_row_t *row, int i, char *out, size_t outlen)
{
	const char *p = row->units;

	while (i-- > 0 && (p = strchr(p, ' ')))
		p++;
	snprintf(out, outlen, "%.*s", p ? (int)strcspn(p, " ") : 0, p ? p : "");
}

static int answers_agree(const char *a, const char *b)
{
	char *ea, *eb;
	double x = strtod(a, &ea), y = strtod(b, &eb), scale;

	if (ea == a || eb == b || strcmp(ea, eb) != 0)
		return strcmp(a, b) == 0;
	scale = x < 0 ? -x : x;
	/* Four displayed digits, after a six-digit round trip: 0.2% is the
	 * rounding, and nothing a wrong unit produces is that close. */
	return (x - y < 0 ? y - x : x - y) <= 2e-3 * (scale > 1e-9 ? scale : 1e-9);
}

/*
 * Whether `got`, the value the check passed to the solver, is the number the
 * question wrote: `written` in `unit`, converted on its own into `slot`. A
 * shared rounding that drifts from it, as Fahrenheit did near absolute zero,
 * fails here even when the solver answers.
 */
static int value_faithful(const char *got, const char *written,
                          const char *unit, const char *slot)
{
	char ref[32];
	double g, r;

	if (quantity_convert(written, unit, slot, ref, (int)sizeof ref) < 0)
		return 1;   /* nothing independent to compare against */
	g = strtod(got, NULL);
	r = strtod(ref, NULL);
	return (g - r < 0 ? r - g : g - r) <= 1e-3 * (r < 0 ? -r : r) + 1e-12;
}

static struct {
	long tried, converted, ambiguous, refused, broken, inexact, failed;
	long drifted;
} trip;

/* Significant digits as written: "0.0250" has three, "300" three. */
static int sig_digits(const char *s)
{
	int n = 0, lead = 1;

	for (; *s && *s != 'e' && *s != 'E'; s++)
		if (*s >= '0' && *s <= '9') {
			if (*s != '0')
				lead = 0;
			if (!lead)
				n++;
		}
	return n ? n : 1;
}

/*
 * The question rewritten so argument `i` is written in unit `to`, and the
 * call the model would copy from it.
 *
 * When the rewrite is exact ("20 m/s" as "72 km/h") it is the same question,
 * so the converted call must answer what the original did. When it is not
 * ("3 m" as 118.110 inch), the number is rounded to the digits the original
 * was written with, "118 inch", which is a different but realistic question:
 * there the converted call must reach an answer at all, because a solver that
 * refuses or overflows on it is exactly what a student typing it would hit.
 */
/* Where `arg` is written followed by the unit `slot`, or NULL. */
static const char *tagged_at(const char *question, const char *arg,
                             const char *slot)
{
	size_t n = strlen(arg);

	for (const char *p = question; (p = strstr(p, arg)) != NULL; p++) {
		const char *u;

		if ((p > question && (p[-1] == '-' || p[-1] == '.')) ||
		    !grounding_number_boundary(question, p, (int)n))
			continue;
		u = quantity_unit_after(p + n, NULL);
		if (u && strcmp(u, slot) == 0)
			return p;
	}
	return NULL;
}

static void round_trip(const char *question, char **f, int nf, int i,
                       const char *slot)
{
	const char *arg = f[i], *p = tagged_at(question, f[i], slot), *to;
	size_t n = strlen(arg);

	if (!p)
		return;
	for (int k = 0; (to = quantity_unit_alternative(slot, k)) != NULL; k++) {
		char value[32], q[1200], call[256], out[256], want[128], got[128];
		const char *args[8], *conv[8];
		char work[256], *g[12];
		int span, len, rc, ng;

		int exact = quantity_convert(arg, slot, to, value, (int)sizeof value);

		if (exact < 0)
			continue;
		exact = exact == 0;
		if (!exact)
			snprintf(value, sizeof value, "%.*g", sig_digits(arg),
			         strtod(value, NULL));
		quantity_unit_after(p + n, &span);
		snprintf(q, sizeof q, "%.*s%s %s%s", (int)(p - question), question,
		         value, to, p + n + span);
		len = snprintf(call, sizeof call, "CALL %s", f[1]);
		for (int j = 2; j < nf; j++)
			len += snprintf(call + len, sizeof call - (size_t)len, " %s",
			                j == i ? value : f[j]);
		trip.tried++;
		rc = quantity_check_call(call, q, out, (int)sizeof out);
		if (rc == QUANTITY_SAME) {
			trip.ambiguous++;   /* the new number collides with another */
			continue;
		}
		if (rc == QUANTITY_MISMATCH) {
			if (trip.refused++ < 5)
				printf("refused: %s | %s\n", q, call);
			continue;
		}
		trip.converted++;
		for (int j = 2; j < nf; j++)
			args[j - 2] = f[j];
		snprintf(work, sizeof work, "%s", out);
		ng = split_args(work, g, 12);
		if (ng == nf && !value_faithful(g[i], value, to, slot) &&
		    trip.drifted++ < 10)
			printf("drifted: %s | %s -> %s\n", q, call, out);
		for (int j = 2; j < ng; j++)
			conv[j - 2] = g[j];
		if (solvers_solve(f[1], args, nf - 2, want, sizeof want) != 0)
			continue;
		got[0] = '\0';
		rc = ng == nf ? solvers_solve(g[1], conv, ng - 2, got, sizeof got) : -1;
		if (exact && (rc != 0 || !answers_agree(want, got))) {
			if (trip.broken++ < 10)
				printf("broken: %s | %s -> %s answers \"%s\", wanted \"%s\"\n",
				       q, call, out, got, want);
		} else if (!exact) {
			trip.inexact++;
			if (rc != 0 && trip.failed++ < 10)
				printf("solver refused: %s | %s -> %s\n", q, call, out);
		}
	}
}

static struct {
	long tried, converted, ambiguous, refused, broken, inexact, failed;
	long drifted;
} group;

/*
 * Every number written in `slot` rewritten into `to` at once: "3 kg and 7 kg"
 * as "6.61 lb and 15.4 lb". This is where the arguments of one call stop being
 * independent, so it is where shared units and shared rounding are tested;
 * one number at a time never reaches them.
 */
static void round_trip_group(const char *question, char **f, int nf,
                             const rel_row_t *row, const char *slot)
{
	const char *at[8], *to;
	int member[8], m = 0;

	for (int i = 2; i < nf; i++) {
		char u[24];

		slot_unit(row, i - 2, u, sizeof u);
		if (strcmp(u, slot) != 0 || f[i][0] == '-' ||
		    !(at[m] = tagged_at(question, f[i], slot)))
			continue;
		for (int k = 0; k < m; k++)
			if (at[k] == at[m])
				return;   /* two arguments read from one number */
		member[m++] = i;
	}
	if (m < 2)
		return;
	/* Rewrite left to right, so order the members by where they are. */
	for (int a = 0; a < m; a++)
		for (int b = a + 1; b < m; b++)
			if (at[b] < at[a]) {
				const char *t = at[a];
				int ti = member[a];

				at[a] = at[b];
				at[b] = t;
				member[a] = member[b];
				member[b] = ti;
			}
	for (int k = 0; (to = quantity_unit_alternative(slot, k)) != NULL; k++) {
		char value[8][32], q[1200], call[256], out[256], want[128], got[128];
		char work[256], *g[12];
		const char *args[8], *conv[8], *from = question;
		int exact = 1, len = 0, ng, rc, ok = 1;

		for (int a = 0; a < m && ok; a++) {
			int i = member[a], span;

			rc = quantity_convert(f[i], slot, to, value[a], (int)sizeof value[a]);
			if (rc < 0) {
				ok = 0;
				break;
			}
			if (rc == 1) {
				exact = 0;
				snprintf(value[a], sizeof value[a], "%.*g", sig_digits(f[i]),
				         strtod(value[a], NULL));
			}
			quantity_unit_after(at[a] + strlen(f[i]), &span);
			len += snprintf(q + len, sizeof q - (size_t)len, "%.*s%s %s",
			                (int)(at[a] - from), from, value[a], to);
			from = at[a] + strlen(f[i]) + span;
		}
		if (!ok)
			continue;
		snprintf(q + len, sizeof q - (size_t)len, "%s", from);
		len = snprintf(call, sizeof call, "CALL %s", f[1]);
		for (int j = 2; j < nf; j++) {
			const char *v = f[j];

			for (int a = 0; a < m; a++)
				if (member[a] == j)
					v = value[a];
			len += snprintf(call + len, sizeof call - (size_t)len, " %s", v);
		}
		group.tried++;
		rc = quantity_check_call(call, q, out, (int)sizeof out);
		if (rc == QUANTITY_SAME) {
			group.ambiguous++;
			continue;
		}
		if (rc == QUANTITY_MISMATCH) {
			if (group.refused++ < 5)
				printf("group refused: %s | %s\n", q, call);
			continue;
		}
		group.converted++;
		for (int j = 2; j < nf; j++)
			args[j - 2] = f[j];
		snprintf(work, sizeof work, "%s", out);
		ng = split_args(work, g, 12);
		for (int a = 0; a < m && ng == nf; a++)
			if (!value_faithful(g[member[a]], value[a], to, slot)) {
				if (group.drifted++ < 10)
					printf("group drifted: %s | %s -> %s\n", q, call, out);
				break;
			}
		for (int j = 2; j < ng; j++)
			conv[j - 2] = g[j];
		if (solvers_solve(f[1], args, nf - 2, want, sizeof want) != 0)
			continue;
		got[0] = '\0';
		rc = ng == nf ? solvers_solve(g[1], conv, ng - 2, got, sizeof got) : -1;
		if (exact && (rc != 0 || !answers_agree(want, got))) {
			if (group.broken++ < 10)
				printf("group broken: %s | %s -> %s answers \"%s\", wanted "
				       "\"%s\"\n", q, call, out, got, want);
		} else if (!exact) {
			group.inexact++;
			if (rc != 0 && group.failed++ < 10)
				printf("group solver refused: %s | %s -> %s\n", q, call, out);
		}
	}
}

static int corpus(void)
{
	char line[1024], out[256];
	long rows = 0, changed = 0, swaps = 0, caught = 0;

	while (fgets(line, sizeof line, stdin)) {
		char *tab = strchr(line, '\t'), *nl, *f[12], work[256];
		int nf;

		if (!tab)
			continue;
		*tab = '\0';
		if ((nl = strchr(tab + 1, '\n')))
			*nl = '\0';
		rows++;
		if (quantity_check_call(tab + 1, line, out, (int)sizeof out) !=
		    QUANTITY_SAME) {
			if (changed < 20)
				printf("changed: %s | %s -> %s\n", line, tab + 1, out);
			changed++;
		}

		/* The failure this exists to catch: two arguments in each other's
		 * places. Measured, not gated; a swap of two values of one kind is
		 * invisible to units by construction. */
		snprintf(work, sizeof work, "%s", tab + 1);
		nf = split_args(work, f, 12);
		{
			const rel_row_t *row = nf >= 2 ? row_named(f[1]) : NULL;

			for (int i = 2; row && i < nf && nf - 2 == row->arity; i++) {
				char slot[24];

				slot_unit(row, i - 2, slot, sizeof slot);
				if (strcmp(slot, "-") != 0 && f[i][0] != '-')
					round_trip(line, f, nf, i, slot);
			}
			for (int i = 2; row && i < nf && nf - 2 == row->arity; i++) {
				char slot[24], earlier[24];
				int seen = 0;

				slot_unit(row, i - 2, slot, sizeof slot);
				for (int j = 2; j < i; j++) {
					slot_unit(row, j - 2, earlier, sizeof earlier);
					seen |= strcmp(earlier, slot) == 0;
				}
				if (strcmp(slot, "-") != 0 && !seen)
					round_trip_group(line, f, nf, row, slot);
			}
		}
		for (int i = 2; i < nf; i++)
			for (int j = i + 1; j < nf; j++) {
				char swapped[256];
				int len;

				if (strcmp(f[i], f[j]) == 0)
					continue;
				len = snprintf(swapped, sizeof swapped, "CALL %s", f[1]);
				for (int k = 2; k < nf; k++)
					len += snprintf(swapped + len, sizeof swapped - (size_t)len,
					                " %s", k == i ? f[j] : k == j ? f[i] : f[k]);
				swaps++;
				if (quantity_check_call(swapped, line, out, (int)sizeof out) ==
				    QUANTITY_MISMATCH)
					caught++;
			}
	}
	printf("swapped arguments: %ld tried, %ld refused (%.1f%%)\n", swaps, caught,
	       swaps ? 100.0 * (double)caught / (double)swaps : 0.0);
	printf("other units: %ld rewritten, %ld converted, %ld left alone, "
	       "%ld refused; exact rewrites answered wrong: %ld; rounded rewrites "
	       "the solver could not answer: %ld of %ld\n", trip.tried,
	       trip.converted, trip.ambiguous, trip.refused, trip.broken,
	       trip.failed, trip.inexact);
	printf("same-unit groups: %ld rewritten, %ld converted, %ld left alone, "
	       "%ld refused; exact rewrites answered wrong: %ld; rounded rewrites "
	       "the solver could not answer: %ld of %ld\n", group.tried,
	       group.converted, group.ambiguous, group.refused, group.broken,
	       group.failed, group.inexact);
	printf("converted values off their own exact conversion: %ld single, "
	       "%ld in groups\n", trip.drifted, group.drifted);
	printf("correct calls: %ld rows, %ld changed\n", rows, changed);
	/* The rounded rewrites the solver cannot answer are reported, not gated:
	 * the exact solvers refuse many plain decimals with no unit involved
	 * ("free_fall 9.9 9.81"), which is their limit, not this module's. */
	return rows > 0 && changed == 0 && trip.broken == 0 && trip.refused == 0 &&
	       group.broken == 0 && group.refused == 0 && trip.drifted == 0 &&
	       group.drifted == 0 ? 0 : 1;
}

int main(int argc, char **argv)
{
	if (argc > 1 && strcmp(argv[1], "--cases") == 0)
		return corpus();
	fixed_cases();
	zero_cases();
	if (failures) {
		printf("%d failures\n", failures);
		return 1;
	}
	printf("all checks passed\n");
	return 0;
}
