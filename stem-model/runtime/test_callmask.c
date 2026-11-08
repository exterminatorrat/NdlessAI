/*
 * Regression tests for constrained decoding.
 *
 * The runtime used to accept any token at any step and reject the malformed
 * call afterwards. These cases drive the mask the way pipeline_answer does,
 * mask first and argmax second, on synthetic logit vectors, so they pin the
 * promise the mask makes: at the first two steps only the header tokens can
 * be chosen, and after that the header cannot be restarted.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bpe.h"
#include "callmask.h"
#include "reltable.h"

#define VOCAB 512

static int32_t logits[VOCAB];
static unsigned char allow[VOCAB];
static int failures;

static void reset_logits(void)
{
	for (int i = 0; i < VOCAB; i++)
		logits[i] = -1000;
}

/* Same order as pipeline_answer: mask the logits, then take the argmax. */
static int masked_argmax(const char *sofar)
{
	int best = 0;

	callmask_next(sofar, allow, VOCAB);
	for (int i = 0; i < VOCAB; i++)
		if (!allow[i])
			logits[i] = INT32_MIN;
	for (int i = 1; i < VOCAB; i++)
		if (logits[i] > logits[best])
			best = i;
	return best;
}

static int allowed_count(const char *sofar)
{
	int n = 0;

	callmask_next(sofar, allow, VOCAB);
	for (int i = 0; i < VOCAB; i++)
		n += allow[i] ? 1 : 0;
	return n;
}

static void expect_choice(const char *sofar, int want, const char *why)
{
	int got = masked_argmax(sofar);

	if (got != want) {
		printf("FAIL %s: chose %d, want %d\n  text: \"%s\"\n",
		       why, got, want, sofar);
		failures++;
	}
}

static void expect_allowed(const char *sofar, int id, int want, const char *why)
{
	int got;

	callmask_next(sofar, allow, VOCAB);
	got = allow[id] ? 1 : 0;
	if (got != want) {
		printf("FAIL %s: token %d is %s\n  text: \"%s\"\n", why, id,
		       got ? "allowed" : "blocked", sofar);
		failures++;
	}
}

static void expect_count(const char *sofar, int want, const char *why)
{
	int got = allowed_count(sofar);

	if (got != want) {
		printf("FAIL %s: %d tokens allowed, want %d\n  text: \"%s\"\n",
		       why, got, want, sofar);
		failures++;
	}
}

static int special_id(const char *name)
{
	for (int i = 0; i < BPE_NUM_SPECIALS; i++)
		if (strcmp(bpe_specials[i], name) == 0)
			return bpe_special_ids[i];
	return -1;
}

static int rel_id(const char *name)
{
	for (int i = 0; i < REL_NUM_RELATIONS; i++)
		if (strcmp(rel_rows[i].name, name) == 0)
			return rel_rows[i].id;
	return -1;
}

/*
 * reltable.h is generated from relations.py, but the token ids it carries
 * have to be the ones bpe.h compiles into the runtime. A table that drifted
 * from the checkpoint's tokenizer would mask plausible tokens and the decode
 * would look healthy while producing calls the solver cannot read.
 */
static void check_table_matches_tokenizer(void)
{
	int call = special_id("CALL");

	if (REL_NUM_RELATIONS != BPE_NUM_SPECIALS - 5) {
		printf("FAIL reltable has %d relations, bpe.h carries %d specials\n",
		       REL_NUM_RELATIONS, BPE_NUM_SPECIALS);
		failures++;
	}
	for (int i = 0; i < REL_NUM_RELATIONS; i++) {
		int idx = special_id(rel_rows[i].name);

		if (idx != rel_rows[i].id) {
			printf("FAIL %s is %d in bpe.h, %d in reltable.h\n",
			       rel_rows[i].name, idx, rel_rows[i].id);
			failures++;
		}
		if (rel_rows[i].arity < 1) {
			printf("FAIL %s has arity 0\n", rel_rows[i].name);
			failures++;
		}
		if (rel_rows[i].id != REL_FIRST_ID + i) {
			printf("FAIL %s has id %d, tokenizer.py assigns %d\n",
			       rel_rows[i].name, rel_rows[i].id, REL_FIRST_ID + i);
			failures++;
		}
		if (i && strcmp(rel_rows[i - 1].name, rel_rows[i].name) >= 0) {
			printf("FAIL relations out of sorted() order at %s\n",
			       rel_rows[i].name);
			failures++;
		}
	}
	/* The one token allowed on an empty decode must be the real CALL. */
	expect_count("", 1, "empty decode allows exactly one token");
	if (call < 0) {
		printf("FAIL bpe.h has no CALL special\n");
		failures++;
	} else {
		expect_allowed("", call, 1, "empty decode allows CALL");
	}
}

int main(void)
{
	const int call = special_id("CALL");
	const int ohm_i = rel_id("ohm_i");
	const int quad = rel_id("quad_solve");

	if (call < 0 || ohm_i < 0 || quad < 0) {
		printf("FAIL reltable.h is missing CALL, ohm_i or quad_solve\n");
		return 1;
	}

	check_table_matches_tokenizer();

	/* (a) Step 0. The model would rather emit a digit, the mask still makes
	 * CALL the only legal opening token. */
	reset_logits();
	logits[(int)'5'] = 900;
	logits[quad] = 800;
	logits[call] = 1;
	expect_choice("", call, "step 0 forces CALL over the preferred digit");

	/* (b) Step 1. A space or digit is the model's preference; only a
	 * relation may follow the header. */
	reset_logits();
	logits[(int)'7'] = 900;
	logits[(int)' '] = 650;
	logits[ohm_i] = 700;
	logits[quad] = 100;
	expect_choice("CALL", ohm_i, "step 1 picks a relation, not the digit");
	expect_count("CALL", REL_NUM_RELATIONS + 1,
		     "step 1 allows the relations and one separator");
	expect_count("CALL   ", REL_NUM_RELATIONS,
		     "trailing spaces do not end the header");
	expect_allowed("CALL", (int)'7', 0, "step 1 blocks digits");
	expect_allowed("CALL", call, 0, "step 1 blocks a second CALL");

	/* The separator is what keeps "CALL" and the relation name as two words:
	 * run_call_text() splits on spaces, so a decode that jams them together
	 * is rejected no matter how good the rest of the call is. */
	expect_allowed("CALL", (int)' ', 1, "separator after the header");
	expect_allowed("CALL ", (int)' ', 0, "only one separator");
	reset_logits();
	logits[(int)' '] = 600;
	logits[quad] = 100;
	expect_choice("CALL", (int)' ', "the trained separator survives step 1");

	/* (c) Step 2. Arguments start here, so CALL and every relation id are
	 * gone: the header cannot be restarted or renamed mid-call. */
	reset_logits();
	logits[ohm_i] = 900;
	logits[call] = 890;
	logits[(int)'1'] = 100;
	expect_choice("CALL ohm_i", (int)'1',
		      "step 2 cannot repeat the relation or restart the call");
	expect_allowed("CALL ohm_i", call, 0, "step 2 blocks CALL");
	expect_allowed("CALL ohm_i", ohm_i, 0, "step 2 blocks the relation again");
	expect_allowed("CALL ohm_i", quad, 0, "step 2 blocks any relation");
	expect_count("CALL ohm_i", VOCAB - REL_NUM_RELATIONS - 1,
		     "step 2 blocks exactly CALL and the relations");

	/* (d) Numbers are untouched by the blacklist: digits, sign, decimal
	 * point and space all stay legal for every argument token. */
	reset_logits();
	logits[(int)'.'] = 500;
	logits[(int)'-'] = 400;
	logits[(int)'9'] = 300;
	expect_choice("CALL ohm_i 1", (int)'.', "decimal point stays legal");
	expect_allowed("CALL ohm_i 1", (int)'9', 1, "digit stays legal");
	expect_allowed("CALL ohm_i 1", (int)'-', 1, "sign stays legal");
	expect_allowed("CALL ohm_i 1", (int)' ', 1, "separator stays legal");
	expect_allowed("CALL ohm_i 1", (int)'.', 1, "decimal point stays legal");

	/* A header that already went wrong is not reopened: only the exact
	 * "CALL" prefix gets the relation whitelist. */
	expect_count("CALLS", VOCAB - REL_NUM_RELATIONS - 1,
		     "a longer word is not the header");
	expect_count("CALL ohm_i 12", VOCAB - REL_NUM_RELATIONS - 1,
		     "later steps keep the same blacklist");

	if (failures) {
		printf("%d call mask case(s) failed\n", failures);
		return 1;
	}
	printf("callmask: all cases passed\n");
	return 0;
}
