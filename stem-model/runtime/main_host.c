#include <stdio.h>
#include <stdlib.h>

#include "infer.h"

int main(int argc, char **argv)
{
	model_t m;
	state_t *s;
	float *logits;
	int pos = 0, n = 0;

	if (argc < 3) {
		fprintf(stderr, "usage: %s weights.bin <token> [token ...]\n", argv[0]);
		return 2;
	}
	if (model_load(argv[1], &m) < 0) {
		fprintf(stderr, "cannot load %s\n", argv[1]);
		return 1;
	}

	m.seq = argc - 2;
	if (getenv("NSM_SEQ"))
		m.seq = atoi(getenv("NSM_SEQ"));
	if (m.seq < 1)
		m.seq = 1;
	s = state_new(&m);
	logits = malloc((size_t)m.vocab * sizeof(float));
	if (!s || !logits) {
		fprintf(stderr, "out of memory\n");
		return 1;
	}

	printf("config dim=%d layers=%d heads=%d hidden=%d vocab=%d seq=%d\n",
	       m.dim, m.layers, m.heads, m.hidden, m.vocab, m.seq);

	for (int i = 2; i < argc && pos < m.seq; i++) {
		char *end;
		long tok = strtol(argv[i], &end, 10);

		if (*end || tok < 0 || tok >= m.vocab) {
			fprintf(stderr, "bad token: %s\n", argv[i]);
			return 1;
		}
		forward(&m, s, (int)tok, pos++, logits);
		n++;
	}
	if (n == 0) {
		fprintf(stderr, "no tokens\n");
		return 1;
	}

	printf("tokens %d\n", n);
	printf("argmax %d\n", argmax(logits, m.vocab));
	for (int i = 0; i < m.vocab; i++)
		printf("logit %d %.6f\n", i, logits[i]);

	state_free(s);
	model_free(&m);
	free(logits);
	return 0;
}
