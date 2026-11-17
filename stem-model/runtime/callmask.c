#include <string.h>

#include "callmask.h"
#include "grounding.h"

#define RELTABLE_IMPL
#include "reltable.h"

#define RELTABLE_IMPL
#include "reltable.h"

/* tokenizer.py: CALL. Kept as a literal the way pipeline.c keeps its EOS and
 * PAD ids; test_callmask checks it against bpe.h's special table so a
 * tokenizer change cannot leave the mask pointing at the wrong token. */
#define CALL_ID 259

/* The space byte that separates the call's fields. run_call_text() splits the
 * decoded call on it, so "CALL" jammed onto a relation name parses as one
 * unknown word and every call would be rejected. The trained model emits it
 * between CALL and the relation, so the whitelist has to keep it eligible. */
#define SPACE_ID 32

static int spaces_only(const char *s)
{
	while (*s == ' ')
		s++;
	return *s == '\0';
}

void callmask_next(const char *sofar, unsigned char *allow, int vocab)
{
	if (vocab <= 0)
		return;

	memset(allow, 0, (size_t)vocab);
	if (spaces_only(sofar)) {
		if (CALL_ID < vocab)
			allow[CALL_ID] = 1;
		return;
	}

	if (strncmp(sofar, "CALL", 4) == 0 && spaces_only(sofar + 4)) {
		const char *rest = sofar + 4;

		if (*rest == '\0') {
			/* The header is not finished until the separator is out: a
			 * relation chosen here would fuse into "CALLquad_solve",
			 * which the parser reads as one unknown field. */
			if (SPACE_ID < vocab)
				allow[SPACE_ID] = 1;
		} else {
			/* Separator already present, so a second one would only
			 * stall the decode instead of naming a relation. */
			for (int i = 0; i < REL_NUM_RELATIONS; i++)
				if (rel_rows[i].id < vocab)
					allow[rel_rows[i].id] = 1;
		}
		return;
	}

	memset(allow, 1, (size_t)vocab);
	if (CALL_ID < vocab)
		allow[CALL_ID] = 0;
	for (int i = 0; i < REL_NUM_RELATIONS; i++)
		if (rel_rows[i].id < vocab)
			allow[rel_rows[i].id] = 0;
}

/* END OF TEXT and FILL stay eligible so a confused decode can still stop; it
 * is the caller's parse that rejects the truncated call. */
#define TOKEN_EOT 258
#define TOKEN_FILL 256

static int free_value(const char *token)
{
	return !strcmp(token, "0") || !strcmp(token, "1");
}

/* Splits the text after "CALL name " on spaces, in place. Returns the number of
 * tokens and leaves `partial` pointing at the unfinished one, or at "". */
static int split_args(char *text, char *words[], int max, const char **partial)
{
	int n = 0, ends_space = 0;
	char *p = text;

	*partial = "";
	if (*p != '\0')
		ends_space = text[strlen(text) - 1] == ' ';
	while (*p && n < max) {
		while (*p == ' ')
			p++;
		if (*p == '\0')
			break;
		words[n++] = p;
		while (*p && *p != ' ')
			p++;
		if (*p)
			*p++ = '\0';
	}
	if (!ends_space && n > 0)
		*partial = words[n - 1];
	return ends_space ? n : n - 1;
}

/*
 * Every frame of the retry's argument text has to name a span the question
 * writes and that no earlier argument has spent. 0 and 1 are free: polynomial
 * questions write their coefficients implicitly.
 */
void callmask_args(const char *sofar, const char *question, const bpe_tok *tok,
                   unsigned char *allow, int vocab)
{
	grounding_span_t spans[GROUNDING_MAX_SPANS];
	int used[GROUNDING_MAX_SPANS];
	char work[128];
	char relation[32];
	char *words[10];
	const char *args, *sep, *partial;
	int nspans, complete, arity;

	if (!allow || vocab <= 0)
		return;
	memset(allow, 1, (size_t)vocab);
	/* The header is the previous mask's business: nothing to add here. */
	if (!sofar || !question || strncmp(sofar, "CALL ", 5) != 0)
		return;
	sep = strchr(sofar + 5, ' ');
	if (!sep)
		return; /* the relation is still being decoded */
	args = sep + 1;
	if (strlen(args) >= sizeof work)
		return;
	memcpy(work, args, strlen(args) + 1);
	{
		size_t rlen = (size_t)(sep - (sofar + 5));

		if (rlen >= sizeof relation)
			return;
		memcpy(relation, sofar + 5, rlen);
		relation[rlen] = '\0';
	}
	nspans = grounding_spans(question, spans, GROUNDING_MAX_SPANS);
	if (nspans <= 0)
		return;

	for (int i = 0; i < GROUNDING_MAX_SPANS; i++)
		used[i] = 0;
	complete = split_args(work, words, 10, &partial);
	/*
	 * The number of slots is the relation's arity, so a call that has not
	 * filled them all has not finished and one that has filled them must stop.
	 * Without this a masked decode ends the call at the first argument it can
	 * say and the solver gets a call of the wrong length.
	 */
	arity = -1;
	for (int i = 0; i < REL_NUM_RELATIONS; i++)
		if (!strcmp(rel_rows[i].name, relation)) {
			arity = rel_rows[i].arity;
			break;
		}
	/*
	 * Spend what the finished arguments already name, left to right. A token
	 * that matches no span means this is not a plain argument list (a plan's
	 * "CALL" or ";", or a call the first decode already took off the rails):
	 * leave the decode unconstrained rather than deadlock it.
	 */
	for (int w = 0; w < complete; w++) {
		int hit = -1;

		if (free_value(words[w]))
			continue;
		for (int s = 0; s < nspans; s++)
			if (!used[s] && !strcmp(spans[s].text, words[w])) {
				hit = s;
				break;
			}
		if (hit < 0) {
			memset(allow, 1, (size_t)vocab);
			return;
		}
		used[hit] = 1;
	}

	memset(allow, 0, (size_t)vocab);
	if (arity >= 0 && complete >= arity) {
		/* Every slot is filled: the call ends here, not one argument later. */
		if (TOKEN_EOT < vocab)
			allow[TOKEN_EOT] = 1;
		if (TOKEN_FILL < vocab)
			allow[TOKEN_FILL] = 1;
		return;
	}
	/* A slot is still owed, so the decode may not stop yet. */
	if (arity < 0) {
		if (TOKEN_EOT < vocab)
			allow[TOKEN_EOT] = 1;
		if (TOKEN_FILL < vocab)
			allow[TOKEN_FILL] = 1;
	}
	for (int s = 0; s < nspans; s++) {
		size_t plen = strlen(partial);
		const char *rest;

		if (used[s] || plen > strlen(spans[s].text) ||
		    strncmp(spans[s].text, partial, plen) != 0)
			continue;
		rest = spans[s].text + plen;
		if (*rest == '\0') {
			allow[SPACE_ID] = 1; /* the argument is complete */
			continue;
		}
		for (int id = 0; id < vocab; id++) {
			char text[GROUNDING_SPAN_LEN];

			if (allow[id])
				continue;
			if (bpe_token_bytes(tok, (uint16_t)id, text,
			                    (int)sizeof text) < 0)
				continue;
			/* A prefix, not the whole remainder: "34" may be two
			 * tokens in this vocabulary, and a number is assembled
			 * one step at a time. */
			size_t tl = strlen(text);

			if (tl > 0 && tl <= strlen(rest) &&
			    strncmp(text, rest, tl) == 0)
				allow[id] = 1;
		}
	}
	if (free_value(partial)) {
		allow[SPACE_ID] = 1;
	} else if (*partial == '\0') {
		for (int id = 0; id < vocab; id++) {
			char text[GROUNDING_SPAN_LEN];

			if (allow[id])
				continue;
			if (bpe_token_bytes(tok, (uint16_t)id, text,
			                    (int)sizeof text) < 0)
				continue;
			if (!strcmp(text, "0") || !strcmp(text, "1"))
				allow[id] = 1;
		}
	}
	/* A partial argument nothing can continue is not worth a dead decode. */
	{
		int any = 0;

		for (int id = 0; id < vocab; id++)
			if (allow[id])
				any = 1;
		if (!any)
			memset(allow, 1, (size_t)vocab);
	}
}
