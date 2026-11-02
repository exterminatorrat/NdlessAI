#ifndef INFER_H
#define INFER_H

#include <stdint.h>
#include <stddef.h>

#define QT_GROUP 32
#define QT_SCALE_BITS 24

typedef struct {
	int dim, layers, heads, hidden, vocab, seq;
	int8_t *tok_emb;
	int32_t *tok_emb_s;
	int8_t *wq, *wk, *wv, *wo, *w1, *w2, *w3;
	int32_t *wq_s, *wk_s, *wv_s, *wo_s, *w1_s, *w2_s, *w3_s;
	float *n1, *n2, *norm;
} model_t;

typedef struct {
	const model_t *m;
	float *k, *v;
	float *x, *xb, *xn, *q, *att, *scores, *hb, *hb2;
} state_t;

int model_load(const char *path, model_t *m);
void model_free(model_t *m);
state_t *state_new(const model_t *m);
void state_free(state_t *s);
void forward(const model_t *m, state_t *s, int token, int pos, float *logits);
int argmax(const float *v, int n);

#endif
