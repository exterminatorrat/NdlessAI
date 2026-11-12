/*
 * Host tests for the relations the decode weighed.
 *
 * pipeline_answer used to keep only the argmax of the relation step, so a
 * question one logit away from another formula looked exactly like a confident
 * answer. The alternative list is what the graphing screen dims, and it is only
 * worth drawing if it agrees with the answer that came back. So the winner is
 * pinned to the relation the emitted call actually used, read back through the
 * table the mask ranks, rather than to a hard-coded name the next model would
 * break.
 *
 * The closed forms matter as much as the words: a list left over from the
 * previous question would draw runners-up for a formula that was never weighed,
 * so every path that answers before inference runs must report nothing.
 *
 *   ./test_alt [weights.bin]
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pipeline.h"
#include "reltable.h"

#define ROOM 8

static int failures;
static pipeline_t p;

static void fail(const char *why, int got, int want)
{
	printf("FAIL %s: got %d, want %d\n", why, got, want);
	failures++;
}

static int is_relation_token(uint16_t token)
{
	for (int i = 0; i < REL_NUM_RELATIONS; i++)
		if (rel_rows[i].id == token)
			return 1;
	return 0;
}

/* The token of the relation the emitted call names, so the winner can be
 * compared with what the decode actually produced rather than with a guess. */
static int call_relation_token(const char *call)
{
	const char *start, *end;
	char name[64];
	size_t n;

	if (strncmp(call, "CALL ", 5) != 0)
		return -1;
	start = call + 5;
	end = strchr(start, ' ');
	n = end ? (size_t)(end - start) : strlen(start);
	if (n == 0 || n >= sizeof name)
		return -1;
	memcpy(name, start, n);
	name[n] = '\0';
	for (int i = 0; i < REL_NUM_RELATIONS; i++)
		if (strcmp(rel_rows[i].name, name) == 0)
			return rel_rows[i].id;
	return -1;
}

static void canary(pipeline_alt_t *alts, int n)
{
	for (int i = 0; i < n; i++) {
		alts[i].token = 0xFFFF;
		alts[i].margin = -1;
	}
}

static void expect_model_answer(const char *question)
{
	char answer[256], call[256];
	pipeline_alt_t alts[ROOM];
	int n, want;

	if (pipeline_answer(&p, question, answer, (int)sizeof answer,
	                    call, (int)sizeof call) != PIPELINE_OK) {
		printf("FAIL %s: no answer\n", question);
		failures++;
		return;
	}

	want = call_relation_token(call);
	if (want < 0) {
		printf("FAIL %s: call \"%s\" names no known relation\n",
		       question, call);
		failures++;
		return;
	}

	/* Twice as many slots as the list has: the tail must stay untouched
	 * rather than be filled with whatever the array held. */
	canary(alts, ROOM);
	n = pipeline_alternatives(&p, alts, ROOM);
	/* The mask always leaves every relation eligible at this step, so the
	 * list must be full: fewer entries would mean the feature quietly
	 * degraded to the argmax it replaced. */
	if (n != PIPELINE_ALTS) {
		printf("FAIL %s: %d alternatives, wanted %d\n",
		       question, n, PIPELINE_ALTS);
		failures++;
		return;
	}
	if (alts[0].token != want)
		fail("the winner is the emitted relation", alts[0].token, want);
	if (alts[0].margin != 0)
		fail("the winner's margin is zero", alts[0].margin, 0);

	for (int i = 0; i < n; i++) {
		int dup = 0;

		if (!is_relation_token(alts[i].token)) {
			printf("FAIL %s: entry %d is not a relation token\n",
			       question, i);
			failures++;
		}
		if (alts[i].margin < 0)
			fail("margin is non-negative", alts[i].margin, 0);
		if (i > 0 && alts[i].margin < alts[i - 1].margin)
			fail("entries are sorted by margin", alts[i].margin,
			     alts[i - 1].margin);
		for (int j = 0; j < i; j++)
			dup |= alts[j].token == alts[i].token;
		if (dup) {
			printf("FAIL %s: entry %d repeats a token\n", question, i);
			failures++;
		}
	}
	for (int i = n; i < ROOM; i++)
		if (alts[i].token != 0xFFFF || alts[i].margin != -1) {
			printf("FAIL %s: wrote entry %d past the count\n",
			       question, i);
			failures++;
		}

	/* A smaller buffer takes the prefix and nothing else. */
	canary(alts, ROOM);
	if (pipeline_alternatives(&p, alts, 1) != 1 ||
	    alts[0].token != want || alts[1].token != 0xFFFF) {
		printf("FAIL %s: truncation to one entry\n", question);
		failures++;
	}
	canary(alts, ROOM);
	if (pipeline_alternatives(&p, alts, ROOM) != n || alts[0].token != want) {
		printf("FAIL %s: the list changed between calls\n", question);
		failures++;
	}
}

static void expect_none(const char *text)
{
	char answer[256], call[256];
	pipeline_alt_t alts[ROOM];
	int n;

	if (pipeline_answer(&p, text, answer, (int)sizeof answer,
	                    call, (int)sizeof call) != PIPELINE_OK) {
		printf("FAIL %s: no closed-form answer\n", text);
		failures++;
		return;
	}
	canary(alts, ROOM);
	n = pipeline_alternatives(&p, alts, ROOM);
	if (n != 0) {
		printf("FAIL %s: reported %d stale alternatives\n", text, n);
		failures++;
	}
	if (alts[0].token != 0xFFFF || alts[0].margin != -1) {
		printf("FAIL %s: wrote an entry when there was none\n", text);
		failures++;
	}
	if (pipeline_alternatives(&p, alts, 0) != 0) {
		printf("FAIL %s: reported entries for a zero-sized buffer\n", text);
		failures++;
	}
}

int main(int argc, char **argv)
{
	const char *weights = argc > 1 ? argv[1] : "../weights.bin";

	if (pipeline_init(&p, weights, 64) < 0) {
		printf("cannot load %s\n", weights);
		return 1;
	}

	expect_model_answer("What is the kinetic energy of a 5 kg object at 3 m/s?");
	expect_model_answer("A 12 V battery is across a 4 ohm resistor. "
	                    "What is the current?");

	/* Every closed form answers before the model runs, so the list has to be
	 * empty even though the two questions above filled it. */
	expect_none("2+2");
	expect_none("det([1,2;3,4])");
	expect_none("d/dx(x^3)");
	expect_none("integrate(x^2,0,3)");
	expect_none("series(sin(x),0,5)");
	expect_none("ode(2y,0,1,1,64)");
	expect_none("desolve(2y,0,1)");

	pipeline_free(&p);

	if (failures) {
		printf("%d failure(s)\n", failures);
		return 1;
	}
	printf("alternatives: all cases passed\n");
	return 0;
}
