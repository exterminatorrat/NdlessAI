/*
 * Multi-step plans: the chains a hard question needs.
 *
 * The point of this binary is that a plan is only useful if it is exact, so
 * every case here states the answer the whole chain should produce, not just
 * that it ran.
 */
#include <stdio.h>
#include <string.h>

#include "pipeline.h"
#include "plan.h"
#include "steps.h"

static int failures;

static void check(int cond, const char *what)
{
	if (!cond) {
		printf("FAIL %s\n", what);
		failures++;
	}
}

/* Runs a plan and returns its final answer, or the error. */
static const char *run(const char *text)
{
	static char answer[96];
	plan_t plan;

	answer[0] = '\0';
	if (plan_execute(text, &plan) < 0) {
		snprintf(answer, sizeof answer, "!%s", plan.err);
		return answer;
	}
	snprintf(answer, sizeof answer, "%s", plan.step[plan.n - 1].answer);
	return answer;
}

static void test_chains(void)
{
	/* The ramp sentence, in the smallest form the catalogue allows: the
	 * weight, then the friction that weight causes. */
	check(strcmp(run("CALL newton2 2 9.8 ; CALL friction_force 0.2 <1>"),
	             "3.92 N") == 0,
	      "weight then friction: 2 kg on g = 9.8 with mu 0.2");

	/* Speed at the bottom of a fall, then the energy that speed carries. */
	check(strcmp(run("CALL kinematic_v 0 3 4 ; CALL kinetic_energy 5 <1>"),
	             "360 J") == 0,
	      "velocity then kinetic energy");

	check(strcmp(run("CALL kinematic_v 0 3 4 ; CALL momentum 5 <1>"),
	             "60 kg m/s") == 0,
	      "velocity then momentum");

	/* Three steps: speed, energy, then power as the energy is delivered. */
	check(strcmp(run("CALL kinematic_v 0 2 5 ; CALL kinetic_energy 2 <1> ; "
	                 "CALL power <2> 4"),
	             "25 W") == 0,
	      "velocity to energy to power, three steps");

	/* A student types the shorthand spelling, not "CALL": both are a step. */
	check(strcmp(run("kinematic_v 0 3 4 ; kinetic_energy 5 <1>"), "360 J") == 0,
	      "a plan without the CALL prefix works too");
	check(strcmp(run("newton2 2 9.8 ; friction_force 0.2 <1>"), "3.92 N") == 0,
	      "and so does the mixed spelling");

	/* A reference may appear in any argument position, not just the first. */
	check(strcmp(run("CALL newton2 5 2 ; CALL power_force <1> 3"),
	             "30 W") == 0,
	      "a reference in a later argument slot");
}

static void test_rules(void)
{
	plan_t plan;

	/* Not a plan at all: the caller should treat it as something else. */
	check(plan_execute("2+2", &plan) == -1, "arithmetic is not a plan");
	check(plan_execute("fri 0.4 500", &plan) == -1, "a shorthand is not a plan");

	/* A forward reference is a loop waiting to happen. */
	check(plan_execute("CALL newton2 5 <2> ; CALL kinematic_v 0 1 1", &plan) == -2,
	      "a step cannot use a step that has not run");
	check(strstr(plan.err, "has not run yet") != NULL, "and says why");

	/* Six steps is over the limit; five is fine. */
	check(plan_execute("CALL newton2 1 1 ; CALL newton2 1 1 ; CALL newton2 1 1 ; "
	                   "CALL newton2 1 1 ; CALL newton2 1 1", &plan) == 0,
	      "five steps are allowed");
	check(plan_execute("CALL newton2 1 1 ; CALL newton2 1 1 ; CALL newton2 1 1 ; "
	                   "CALL newton2 1 1 ; CALL newton2 1 1 ; CALL newton2 1 1",
	                   &plan) == -2,
	      "six steps are not");

	/* A step that answers with two numbers cannot feed the next one. */
	check(plan_execute("CALL quad_solve 1 -3 2 ; CALL newton2 <1> 1", &plan) == -2,
	      "a multi-valued answer cannot be chained");
	check(strstr(plan.err, "more than one number") != NULL, "and says why");

	/* A failing step stops the plan, and the message names the step. */
	check(plan_execute("CALL newton2 1 1 ; CALL friction_force 0.2", &plan) == -2,
	      "a step with the wrong argument count fails");
	check(strstr(plan.err, "step 2") != NULL, "and the error names the step");

	check(plan_execute("CALL newton2 1 1 ; nonsense 1", &plan) == -2,
	      "a step that is not a call fails");
}

static void test_working(void)
{
	plan_t plan;
	char out[512];

	check(plan_execute("CALL newton2 2 9.8 ; CALL friction_force 0.2 <1>", &plan) == 0,
	      "the ramp plan runs");
	check(plan_render(&plan, out, (int)sizeof out) == 1, "it renders its working");
	check(strstr(out, "1. F = m*a") != NULL || strstr(out, "1. ") != NULL,
	      "the working numbers its steps");
	check(strstr(out, "3.92 N") != NULL, "and ends on the last answer");
	check(strstr(out, "2. ") != NULL, "with both steps shown");
}

static void test_pipeline(void)
{
	char answer[128];
	char call[128];
	int rc;

	/* A typed plan answers through the same offline path as arithmetic, so a
	 * calculator with no weights file can still solve a chain. */
	rc = pipeline_answer_offline("CALL newton2 2 9.8 ; CALL friction_force 0.2 <1>",
	                             answer, (int)sizeof answer, call, (int)sizeof call);
	check(rc == 0 && strcmp(answer, "3.92 N") == 0, "the pipeline answers a chain");
	check(strncmp(call, "CALL newton2", 12) == 0, "and keeps the plan as the call");

	/* Its working comes through pipeline_steps, which is what the answer pane
	 * prints — the same door a single call goes through. */
	{
		char block[512];

		check(pipeline_steps(call, answer, block, (int)sizeof block) == 1,
		      "the plan's working renders through pipeline_steps");
		check(strstr(block, "2. ") != NULL, "with every step in it");
	}

	/* A failing chain is refused, not half-answered. */
	rc = pipeline_answer_offline("CALL newton2 1 1 ; CALL friction_force 0.2",
	                             answer, (int)sizeof answer, call, (int)sizeof call);
	check(rc == 2, "a broken chain is refused");
}

int main(void)
{
	test_chains();
	test_rules();
	test_working();
	test_pipeline();
	if (failures) {
		printf("%d failure(s)\n", failures);
		return 1;
	}
	printf("plans: all checks passed\n");
	return 0;
}
