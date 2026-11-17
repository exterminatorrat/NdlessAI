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
#include "grounding.h"
#include "reltable.h"

/* The real vocabulary, not a sample of it: a merge token for "44" can sit
 * above the first 512 entries, and scanning a prefix would report no token. */
#define VOCAB BPE_VOCAB_SIZE

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

/* The argument mask, driven through the same body of tests as the header. */
static void mask_args_only(const bpe_tok *tok, const char *sofar,
                           const char *question)
{
	callmask_args(sofar, question, tok, allow, VOCAB);
}

static void expect_arg_allowed(const bpe_tok *tok, const char *sofar,
                               const char *question, int id, int want,
                               const char *why)
{
	mask_args_only(tok, sofar, question);
	if (!!allow[id] != want) {
		printf("FAIL %s: token %d is %s\n", why, id,
		       allow[id] ? "allowed" : "blocked");
		failures++;
	}
}

static int arg_allowed_count(const bpe_tok *tok, const char *sofar,
                             const char *question)
{
	int n = 0;

	mask_args_only(tok, sofar, question);
	for (int i = 0; i < VOCAB; i++)
		n += allow[i] ? 1 : 0;
	return n;
}

/*
 * The rule the retry depends on: only what the question writes, each number
 * spendable once, 0 and 1 free, and no constraint at all before the arguments
 * begin.
 *
 * The assertions are on one-character tokens, which exist in every vocabulary,
 * rather than on a whole two-digit token: whether "88" is one token is the
 * tokenizer's business, whether "8" may begin an argument is this mask's.
 */
static void check_arguments(void)
{
	bpe_tok tok;
	const char *q = "A 12 kg mass moves at 34 m/s.";
	const char *rule = "CALL kinetic_energy ";

	if (bpe_init(&tok) < 0) {
		printf("FAIL bpe_init\n");
		failures++;
		return;
	}

	/* Before the header is through, the argument rule says nothing. */
	mask_args_only(&tok, "", q);
	for (int i = 0; i < VOCAB; i++)
		if (!allow[i]) {
			printf("FAIL an empty decode is not the argument rule's step\n");
			failures++;
			break;
		}

	/* Both numbers in the sentence are reachable, a number it never wrote is not. */
	expect_arg_allowed(&tok, rule, q, '1', 1, "12 is in the question");
	expect_arg_allowed(&tok, rule, q, '3', 1, "34 is in the question");
	expect_arg_allowed(&tok, rule, q, '7', 0, "7 is not a number the question writes");

	/* A spent number is not offered again: 12 cannot be spelled twice, though
	 * the free value 1 may still begin the next argument. */
	expect_arg_allowed(&tok, "CALL kinetic_energy 12 1", q, '2', 0,
	                   "12 has been spent by the first argument");
	expect_arg_allowed(&tok, "CALL kinetic_energy 12 ", q, '3', 1,
	                   "34 is still unspent");

	/* Implicit coefficients are free values, as they are for grounding. */
	expect_arg_allowed(&tok, "CALL deriv_poly3 ", "x^3 + 2x^2 - 6 at x = 4",
	                   '0', 1, "0 is an implicit coefficient");
	expect_arg_allowed(&tok, "CALL deriv_poly3 ", "x^3 + 2x^2 - 6 at x = 4",
	                   '1', 1, "1 is an implicit coefficient");

	/* A space ends a complete argument, and a prefix of one is not complete. */
	mask_args_only(&tok, "CALL kinetic_energy 12", q);
	if (!allow[32]) {
		printf("FAIL a complete argument cannot be closed\n");
		failures++;
	}
	mask_args_only(&tok, "CALL kinetic_energy 3", q);
	if (allow[32]) {
		printf("FAIL a prefix of a number was closed as if complete\n");
		failures++;
	}

	/* A call already off the rails is left alone rather than deadlocked. */
	if (arg_allowed_count(&tok, "CALL kinetic_energy 999 ", q) < VOCAB / 2) {
		printf("FAIL an ungrounded argument does not deadlock the mask\n");
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
	check_arguments();

	/* (a) Step 0. The model would rather emit a digit, the mask still makes
	 * CALL the only legal opening token. */
	reset_logits();
	logits[(int)'5'] = 900;
	logits[quad] = 800;
	logits[call] = 1;
	expect_choice("", call, "step 0 forces CALL over the preferred digit");

	/* (b) Step 1. The header is not finished until its separator is out: a
	 * relation chosen here would fuse into "CALLquad_solve", which
	 * run_call_text reads as one unknown field. So the separator is the only
	 * thing the model may choose, whatever it would prefer. */
	reset_logits();
	logits[ohm_i] = 900;
	logits[quad] = 800;
	logits[(int)'7'] = 700;
	logits[(int)' '] = 100;
	expect_choice("CALL", (int)' ', "step 1 forces the separator, not a relation");
	expect_count("CALL", 1, "bare CALL allows the separator alone");
	expect_allowed("CALL", ohm_i, 0, "bare CALL blocks relations");
	expect_allowed("CALL", quad, 0, "bare CALL blocks every relation");
	expect_allowed("CALL", (int)'7', 0, "bare CALL blocks digits");
	expect_allowed("CALL", call, 0, "bare CALL blocks a second CALL");

	/* (b2) Step 2, separator out: the relations, and only them. */
	reset_logits();
	logits[(int)'7'] = 900;
	logits[(int)' '] = 850;
	logits[ohm_i] = 700;
	logits[quad] = 100;
	expect_choice("CALL ", ohm_i, "step 2 picks a relation, not the digit");
	expect_count("CALL ", REL_NUM_RELATIONS, "step 2 allows only relations");
	expect_count("CALL   ", REL_NUM_RELATIONS,
		     "trailing spaces do not end the header");
	expect_allowed("CALL ", (int)' ', 0, "only one separator");
	expect_allowed("CALL ", (int)'7', 0, "step 2 blocks digits");
	expect_allowed("CALL ", call, 0, "step 2 blocks a second CALL");

	/* (c) Step 3. Arguments start here, so CALL and every relation id are
	 * gone: the header cannot be restarted or renamed mid-call. */
	reset_logits();
	logits[ohm_i] = 900;
	logits[call] = 890;
	logits[(int)'1'] = 100;
	expect_choice("CALL ohm_i", (int)'1',
		      "step 3 cannot repeat the relation or restart the call");
	expect_allowed("CALL ohm_i", call, 0, "step 3 blocks CALL");
	expect_allowed("CALL ohm_i", ohm_i, 0, "step 3 blocks the relation again");
	expect_allowed("CALL ohm_i", quad, 0, "step 3 blocks any relation");
	expect_count("CALL ohm_i", VOCAB - REL_NUM_RELATIONS - 1,
		     "step 3 blocks exactly CALL and the relations");

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

	/* A header that already went wrong is not reopened: only "CALL" plus
	 * its separator gets the relation whitelist. */
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
