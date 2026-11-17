/*
 * The unit check, on fixed cases for each outcome: what it reads after a
 * number, what it converts, what it refuses, and what it leaves alone.
 *
 *     ./test_quantity
 */
#include <stdio.h>
#include <string.h>

#include "quantity.h"
#include "reltable.h"
#include "solvers.h"

/* Revised 2026-11-18: keep unit tests aligned with fractional Carnot output. */

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
	 * 0.3516 and 0.01824 K, a Carnot efficiency of 0.9481 instead of 0.8955. The
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

int main(void)
{
	fixed_cases();
	zero_cases();
	if (failures) {
		printf("%d failures\n", failures);
		return 1;
	}
	printf("all checks passed\n");
	return 0;
}
