/*
 * Typed calls: a name token and numbers, answered with no decoder in the path.
 *
 * The two failures worth testing are both about restraint: a sentence must
 * never be read as a shorthand, and a letter pair that means two relations
 * must be refused rather than resolved by table order.
 */
#include <stdio.h>
#include <string.h>

#include "pipeline.h"
#include "shorthand.h"

static int failures;

static void check(int cond, const char *what)
{
	if (!cond) {
		printf("FAIL %s\n", what);
		failures++;
	}
}

/* The call text shorthand_build produced, or "" when it declined. */
static const char *built(const char *text, int *rc)
{
	static char call[96];

	*rc = shorthand_build(text, call, (int)sizeof call);
	return call;
}

static void test_names(void)
{
	int rc;
	char answer[128];

	check(strcmp(built("friction_force 0.4 500", &rc), "CALL friction_force 0.4 500") == 0 && rc == 0,
	      "the relation's own name builds its call");
	check(strcmp(built("friction-force 0.4 500", &rc), "CALL friction_force 0.4 500") == 0 && rc == 0,
	      "a hyphen stands in for the underscore");
	check(strcmp(built("FRICTION_FORCE 0.4 500", &rc), "CALL friction_force 0.4 500") == 0 && rc == 0,
	      "case does not matter");
	check(strcmp(built("fric 0.4 500", &rc), "CALL friction_force 0.4 500") == 0 && rc == 0,
	      "a unique prefix resolves");
	check(strcmp(built("fri 0.4 500", &rc), "CALL friction_force 0.4 500") == 0 && rc == 0,
	      "three letters is enough when they are unique");
	check(strcmp(built("newton2 5 2", &rc), "CALL newton2 5 2") == 0 && rc == 0,
	      "a one-word name works whole");

	/* Initials, when the name's own parts make them unique: kinematic_v is
	 * "kv", ohm_v is "ov". */
	check(strcmp(built("kv 9 2 8", &rc), "CALL kinematic_v 9 2 8") == 0 && rc == 0,
	      "unique initials resolve");
	check(strcmp(built("ov 5 5", &rc), "CALL ohm_v 5 5") == 0 && rc == 0,
	      "and two relations sharing a first word still differ by theirs");

	/* Signed and exponent arguments survive the trip. */
	check(strcmp(built("newton2 5 -2", &rc), "CALL newton2 5 -2") == 0 && rc == 0,
	      "a negative argument is a number");
	check(strcmp(built("photon_energy_ev 1.86e2", &rc), "CALL photon_energy_ev 1.86e2") == 0 &&
	      rc == 0, "an exponent argument is a number");

	/* And the answer really comes out of the solver. */
	check(pipeline_answer_offline("fri 0.4 500", answer, (int)sizeof answer, NULL, 0) == 0 &&
	      strcmp(answer, "200 N") == 0, "the shorthand answers 200 N");
	check(pipeline_answer_offline("2+2", answer, (int)sizeof answer, NULL, 0) == 0 &&
	      strcmp(answer, "4") == 0, "arithmetic still wins");
}

static void test_restraint(void)
{
	int rc;

	/* Sentences, expressions and arithmetic must fall through to the decoder,
	 * or a student's own words would be silently re-read as a call. */
	check(built("a 5 kg crate slides with friction coefficient 0.4", &rc)[0] == '\0' &&
	      rc == -1, "a sentence is not a shorthand");
	check(built("integrate(x^2)", &rc)[0] == '\0' && rc == -1,
	      "an expression is not a shorthand");
	check(built("2+2", &rc)[0] == '\0' && rc == -1, "arithmetic is not a shorthand");
	check(built("fri", &rc)[0] == '\0' && rc == -1, "a bare name is not a shorthand");
	check(built("nonsense_relation 1 2", &rc)[0] == '\0' && rc == -1,
	      "an unknown name is not a shorthand");

	/* Two letters that begin two relations mean neither of them. */
	check(built("ff 0.4 500", &rc)[0] == '\0' && rc == -1,
	      "initials shared by two relations resolve to neither (free_fall, friction_force)");
	check(built("fr 0.4 500", &rc)[0] == '\0' && rc == -1,
	      "a two-letter prefix is below the threshold");

	/* A real relation with the wrong number of arguments is refused with the
	 * call shown, rather than handed to a decoder that would guess. */
	check(strcmp(built("fri 0.4", &rc), "CALL friction_force 0.4") == 0 && rc == 1,
	      "too few arguments is refused, not guessed");
	check(strcmp(built("fri 0.4 500 1", &rc), "CALL friction_force 0.4 500 1") == 0 && rc == 1,
	      "and so is one argument too many");
}

static void test_offline_path(void)
{
	char answer[128];
	char call[64];
	int rc;

	/* No model, no weights: a typed call still answers, and the call comes
	 * back so the screen can print the working for it. */
	rc = pipeline_answer_offline("kinematic_v 9 2 8", answer, (int)sizeof answer,
	                             call, (int)sizeof call);
	check(rc == 0 && strcmp(answer, "25 m/s") == 0, "the offline path answers");
	check(strcmp(call, "CALL kinematic_v 9 2 8") == 0, "and reports the call it made");

	/* Words with no model are not this function's problem: it says so. */
	rc = pipeline_answer_offline("a 5 kg crate slides", answer, (int)sizeof answer,
	                             call, (int)sizeof call);
	check(rc == 1, "a sentence is left for the model");
	check(call[0] == '\0', "and leaves no call behind");

	/* A relation whose arguments do not fit is refused, with the call shown. */
	rc = pipeline_answer_offline("fri 0.4", answer, (int)sizeof answer,
	                             call, (int)sizeof call);
	check(rc == 2, "a short call is refused");
	check(strcmp(call, "CALL friction_force 0.4") == 0, "with the call it understood");
}

int main(void)
{
	test_names();
	test_restraint();
	test_offline_path();
	if (failures) {
		printf("%d failure(s)\n", failures);
		return 1;
	}
	printf("shorthand: all checks passed\n");
	return 0;
}
