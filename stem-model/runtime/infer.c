#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "infer.h"

#define ROPE_THETA 10000.0f

static void rope(float *v, int pos, int hd)
{
	int half = hd / 2;

	for (int i = 0; i < half; i++) {
		float freq = powf(ROPE_THETA, -(float)i / (float)half);
		float ang = (float)pos * freq;
		float c = cosf(ang), s = sinf(ang);
		float x1 = v[i], x2 = v[i + half];

		v[i] = x1 * c - x2 * s;
		v[i + half] = x1 * s + x2 * c;
	}
}

static void *rd(FILE *f, size_t n)
{
	void *p = malloc(n ? n : 1);

	if (!p || fread(p, 1, n, f) != n) {
		free(p);
		return NULL;
	}
	return p;
}

static int read_hdr(FILE *f, model_t *m)
{
	char magic[4];
	int32_t h[5];

	if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "NSM1", 4) ||
	    fread(h, sizeof(h), 1, f) != 1)
		return -1;

	m->dim = h[0];
	m->layers = h[1];
	m->heads = h[2];
	m->hidden = h[3];
	m->vocab = h[4];
	m->seq = 0;

	if (m->dim % QT_GROUP || m->hidden % QT_GROUP || m->vocab % QT_GROUP)
		return -1;
	if (m->heads < 1 || m->dim % m->heads)
		return -1;
	return 0;
}

static int load_qt(FILE *f, int8_t **w, int32_t **s, int rows, int cols)
{
	size_t n = (size_t)rows * cols;

	*w = rd(f, n);
	*s = rd(f, (size_t)rows * (cols / QT_GROUP) * sizeof(int32_t));
	return (*w && *s) ? 0 : -1;
}

static int load_layers(FILE *f, model_t *m)
{
	int d = m->dim, h = m->hidden, L = m->layers;

	if (load_qt(f, &m->wq, &m->wq_s, L * d, d) < 0 ||
	    load_qt(f, &m->wk, &m->wk_s, L * d, d) < 0 ||
	    load_qt(f, &m->wv, &m->wv_s, L * d, d) < 0 ||
	    load_qt(f, &m->wo, &m->wo_s, L * d, d) < 0 ||
	    load_qt(f, &m->w1, &m->w1_s, L * h, d) < 0 ||
	    load_qt(f, &m->w3, &m->w3_s, L * h, d) < 0 ||
	    load_qt(f, &m->w2, &m->w2_s, L * d, h) < 0)
		return -1;
	m->n1 = rd(f, (size_t)L * d * sizeof(float));
	m->n2 = rd(f, (size_t)L * d * sizeof(float));
	m->norm = rd(f, (size_t)d * sizeof(float));

	return (m->n1 && m->n2 && m->norm) ? 0 : -1;
}

int model_load(const char *path, model_t *m)
{
	FILE *f = fopen(path, "rb");
	int rc;

	if (!f)
		return -1;
	memset(m, 0, sizeof(*m));
	rc = read_hdr(f, m);
	if (rc < 0) {
		fclose(f);
		return -1;
	}
	if (load_qt(f, &m->tok_emb, &m->tok_emb_s, m->vocab, m->dim) < 0 ||
	    load_layers(f, m) < 0) {
		fclose(f);
		model_free(m);
		return -1;
	}
	fclose(f);
	return 0;
}

void model_free(model_t *m)
{
	free(m->tok_emb); free(m->tok_emb_s);
	free(m->wq); free(m->wk); free(m->wv); free(m->wo);
	free(m->w1); free(m->w2); free(m->w3);
	free(m->wq_s); free(m->wk_s); free(m->wv_s); free(m->wo_s);
	free(m->w1_s); free(m->w2_s); free(m->w3_s);
	free(m->n1); free(m->n2); free(m->norm);
	memset(m, 0, sizeof(*m));
}

static void matvec(const int8_t *w, const int32_t *s, const float *x, float *y,
                   int rows, int cols)
{
	int groups = cols / QT_GROUP;

	for (int r = 0; r < rows; r++) {
		const int8_t *wr = w + (size_t)r * cols;
		const int32_t *sr = s + (size_t)r * groups;
		float acc = 0.0f;

		for (int g = 0; g < groups; g++) {
			float part = 0.0f;
			int base = g * QT_GROUP;

			for (int i = 0; i < QT_GROUP; i++)
				part += (float)wr[base + i] * x[base + i];
			acc += part * ((float)sr[g] / (float)(1 << QT_SCALE_BITS));
		}
		y[r] = acc;
	}
}

static void rmsnorm(float *o, const float *x, const float *w, int n)
{
	float ss = 0.0f;

	for (int i = 0; i < n; i++)
		ss += x[i] * x[i];
	ss = 1.0f / sqrtf(ss / n + 1e-5f);
	for (int i = 0; i < n; i++)
		o[i] = w[i] * x[i] * ss;
}

state_t *state_new(const model_t *m)
{
	state_t *s = calloc(1, sizeof(*s));

	if (!s)
		return NULL;
	s->m = m;
	s->k = malloc((size_t)m->layers * m->seq * m->dim * sizeof(float));
	s->v = malloc((size_t)m->layers * m->seq * m->dim * sizeof(float));
	s->x = malloc((size_t)m->dim * sizeof(float));
	s->xb = malloc((size_t)m->dim * sizeof(float));
	s->xn = malloc((size_t)m->dim * sizeof(float));
	s->q = malloc((size_t)m->dim * sizeof(float));
	s->att = malloc((size_t)m->dim * sizeof(float));
	s->scores = malloc((size_t)m->seq * sizeof(float));
	s->hb = malloc((size_t)m->hidden * sizeof(float));
	s->hb2 = malloc((size_t)m->hidden * sizeof(float));

	if (!s->k || !s->v || !s->x || !s->xb || !s->xn || !s->q || !s->att ||
	    !s->scores || !s->hb || !s->hb2) {
		state_free(s);
		return NULL;
	}
	return s;
}

void state_free(state_t *s)
{
	if (!s)
		return;
	free(s->k); free(s->v); free(s->x); free(s->xb); free(s->xn);
	free(s->q); free(s->att); free(s->scores); free(s->hb); free(s->hb2);
	free(s);
}

static void attention(const model_t *m, state_t *s, int l, int pos, float *out)
{
	int d = m->dim, hd = d / m->heads;
	size_t off = (size_t)l * m->seq * d;

	for (int h = 0; h < m->heads; h++) {
		const float *qh = s->q + h * hd;
		float *sc = s->scores;
		float maxs = -INFINITY, sum = 0.0f;

		for (int t = 0; t <= pos; t++) {
			const float *kh = s->k + off + (size_t)t * d + h * hd;
			float dot = 0.0f;

			for (int i = 0; i < hd; i++)
				dot += qh[i] * kh[i];
			sc[t] = dot / sqrtf((float)hd);
			if (sc[t] > maxs)
				maxs = sc[t];
		}
		for (int t = 0; t <= pos; t++) {
			sc[t] = expf(sc[t] - maxs);
			sum += sc[t];
		}
		for (int i = 0; i < hd; i++)
			out[h * hd + i] = 0.0f;
		for (int t = 0; t <= pos; t++) {
			const float *vh = s->v + off + (size_t)t * d + h * hd;
			float p = sc[t] / sum;

			for (int i = 0; i < hd; i++)
				out[h * hd + i] += p * vh[i];
		}
	}
}

void forward(const model_t *m, state_t *s, int token, int pos, float *logits)
{
	int d = m->dim;

	if (token < 0 || token >= m->vocab)
		return;
	if (pos < 0 || pos >= m->seq)
		return;

	const int8_t *er = m->tok_emb + (size_t)token * d;
	const int32_t *es = m->tok_emb_s + (size_t)token * (d / QT_GROUP);
	for (int i = 0; i < d; i++)
		s->x[i] = (float)er[i] * ((float)es[i / QT_GROUP] / (float)(1 << QT_SCALE_BITS));


	for (int l = 0; l < m->layers; l++) {
		size_t nd = (size_t)d * d, nh = (size_t)m->hidden * d;
		size_t nhy = (size_t)d * m->hidden;
		size_t off = (size_t)l * m->seq * d;

		rmsnorm(s->xn, s->x, m->n1 + (size_t)l * d, d);
		matvec(m->wq + l * nd, m->wq_s + l * nd / QT_GROUP, s->xn, s->q, d, d);
		matvec(m->wk + l * nd, m->wk_s + l * nd / QT_GROUP, s->xn,
		       s->k + off + (size_t)pos * d, d, d);
		matvec(m->wv + l * nd, m->wv_s + l * nd / QT_GROUP, s->xn,
		       s->v + off + (size_t)pos * d, d, d);

		for (int h = 0; h < m->heads; h++) {
			rope(s->q + h * (d / m->heads), pos, d / m->heads);
			rope(s->k + off + (size_t)pos * d + h * (d / m->heads), pos, d / m->heads);
		}

		attention(m, s, l, pos, s->att);
		matvec(m->wo + l * nd, m->wo_s + l * nd / QT_GROUP, s->att, s->xb, d, d);
		for (int i = 0; i < d; i++)
			s->x[i] += s->xb[i];

		rmsnorm(s->xn, s->x, m->n2 + (size_t)l * d, d);
		matvec(m->w1 + l * nh, m->w1_s + l * nh / QT_GROUP, s->xn, s->hb, m->hidden, d);
		matvec(m->w3 + l * nh, m->w3_s + l * nh / QT_GROUP, s->xn, s->hb2, m->hidden, d);
		for (int i = 0; i < m->hidden; i++)
			s->hb[i] = (s->hb[i] / (1.0f + expf(-s->hb[i]))) * s->hb2[i];
		matvec(m->w2 + l * nhy, m->w2_s + l * nhy / QT_GROUP, s->hb, s->xb, d, m->hidden);
		for (int i = 0; i < d; i++)
			s->x[i] += s->xb[i];
	}

	rmsnorm(s->xn, s->x, m->norm, d);
	matvec(m->tok_emb, m->tok_emb_s, s->xn, logits, m->vocab, d);
}

int argmax(const float *v, int n)
{
	int best = 0;

	for (int i = 1; i < n; i++)
		if (v[i] > v[best])
			best = i;
	return best;
}
