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

/* The vocabulary the shipped tokenizer compiles: a mask tested against a
 * smaller one passes over tokens the device can actually emit. */
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

	callmask_next(sofar, allow, VOCAB, 0);
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

	callmask_next(sofar, allow, VOCAB, 0);
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

	callmask_next(sofar, allow, VOCAB, 0);
	got = allow[id] ? 1 : 0;
	if (got != want) {
		printf("FAIL %s: token %d is %s\n  text: \"%s\"\n", why, id,
		       got ? "allowed" : "blocked", sofar);
		failures++;
	}
}

/*
 * The same two questions for a chain. Kept beside the single-call helpers so a
 * reader can see the two modes answer the same text differently.
 */
static void expect_chain_allowed(const char *sofar, int chains, int id,
                                 int want, const char *why)
{
	int got;

	callmask_next(sofar, allow, VOCAB, chains);
	got = allow[id] ? 1 : 0;
	if (got != want) {
		printf("FAIL %s: token %d is %s\n  text: \"%s\"\n", why, id,
		       got ? "allowed" : "blocked", sofar);
		failures++;
	}
}

static void expect_chain_count(const char *sofar, int want, const char *why)
{
	int got = 0;

	callmask_next(sofar, allow, VOCAB, 1);
	for (int i = 0; i < VOCAB; i++)
		got += allow[i] ? 1 : 0;
	if (got != want) {
		printf("FAIL %s: %d tokens allowed, want %d\n  text: \"%s\"\n",
		       why, got, want, sofar);
		failures++;
	}
}

/*
 * Whether the decode could emit `chain` token for token. Every token has to be
 * eligible at the moment the decode reaches it, on the text decoded so far —
 * the exact test pipeline_answer makes on every step, replayed against a
 * target the model is supposed to produce.
 */
static void expect_maskable(const char *chain, int chains, int want,
                            const char *why)
{
	bpe_tok tok;
	uint16_t ids[128];
	char sofar[256];
	char where[160];
	int n, used = 0, ok = 1;

	where[0] = '\0';

	if (bpe_init(&tok) < 0) {
		printf("FAIL bpe_init\n");
		failures++;
		return;
	}
	n = bpe_encode(&tok, chain, ids, 128);
	sofar[0] = '\0';
	for (int i = 0; i < n && ok; i++) {
		char piece[64];
		int got = bpe_token_bytes(&tok, ids[i], piece, (int)sizeof piece - 1);

		if (got < 0 || used + got >= (int)sizeof sofar) {
			ok = 0;
			break;
		}
		piece[got] = '\0';
		callmask_next(sofar, allow, VOCAB, chains);
		if (!allow[ids[i]]) {
			snprintf(where, sizeof where, " (blocked at token %d of %d, after \"%s\")",
			         i, n, sofar);
			ok = 0;
			break;
		}
		memcpy(sofar + used, piece, (size_t)got + 1);
		used += got;
	}
	if (ok != want) {
		printf("FAIL %s%s\n", why, where);
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

	/*
	 * (e) A chain. The model is supposed to answer a two-step question with
	 * two calls, and the mask of (c) is what made that impossible: after the
	 * first step every relation id is blocked, so the second call could name
	 * nothing and no amount of training could produce one.
	 */
	{
		const char *chain =
			"CALL newton2 2 9.8 ; CALL friction_force 0.2 <1>";

		expect_maskable(chain, 0, 0,
		                "without chains the second call is unmaskable");
		expect_maskable(chain, 1, 1, "with chains every token is eligible");
		expect_maskable("CALL newton2 2 9.8 ; CALL friction_force 0.2 <1> ; "
		                "CALL work 3.92 2 0 ; CALL power 7.84 2", 1, 1,
		                "and a four-step chain, reference tokens included");

		/* The header reopens after the separator, and only there. */
		expect_chain_allowed("CALL ohm_i 1 ; ", 1, call, 1,
		                     "a separator reopens the header");
		expect_chain_allowed("CALL ohm_i 1 ; CALL ", 1, ohm_i, 1,
		                     "the second header takes a relation");
		expect_chain_allowed("CALL ohm_i 1 ; CALL ", 1, (int)'7', 0,
		                     "and still not a digit");
		expect_chain_allowed("CALL ohm_i 1 ; CALL ohm_i 2 ", 1, call, 0,
		                     "mid-call the header stays closed");
		expect_chain_allowed("CALLS ; ", 1, call, 1,
		                     "a separator inside a wrong word reopens nothing");
		expect_chain_count("CALL ohm_i 1 ; CALL ", REL_NUM_RELATIONS,
		                   "a second header allows only relations");
		expect_chain_count("CALL ohm_i 1 ;", 2,
		                   "a started step allows the header and its space");
		/* Without the flag the text is masked as one long call, exactly as
		 * it was before: this is what keeps today's decode bit-identical. */
		expect_chain_allowed("CALL ohm_i 1 ; CALL ", 0, ohm_i, 0,
		                     "a separated text is still one call by default");
	}

	if (failures) {
		printf("%d call mask case(s) failed\n", failures);
		return 1;
	}
	printf("callmask: all cases passed\n");
	return 0;
}
