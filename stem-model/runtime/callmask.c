#include <string.h>

#include "callmask.h"

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
