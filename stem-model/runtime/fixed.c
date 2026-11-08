#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "fixed.h"
#include "infer.h"

#define EXP_CLAMP 42598
#define LOG2E_Q12 23631
#define EPS_Q24 168

uint32_t fixed_sat_count;

static uint32_t rsq_tab[64];
static uint16_t exp_tab[257];
static uint16_t silu_tab[513];
static uint32_t recip_tab[32];

static int16_t sat16(int64_t v)
{
	if (v > 32767) {
		fixed_sat_count++;
		return 32767;
	}
	if (v < -32768) {
		fixed_sat_count++;
		return -32768;
	}
	return (int16_t)v;
}

static int32_t sat32(int64_t v)
{
	if (v > INT32_MAX)
		return INT32_MAX;
	if (v < INT32_MIN)
		return INT32_MIN;
	return (int32_t)v;
}

static int msb32(uint32_t v)
{
	int b = 0;

	while (v >>= 1)
		b++;
	return b;
}

/* The body below covers exactly one int8 weight group; a wider group would
 * silently contribute only its first 32 weights. */
_Static_assert(QT_GROUP == 32, "dot_group is written out for 32 weights");

/* One int8 weight group against Q12 activations.
 *
 * Exactness: |w| <= 128 and |x| <= 32768, so one term is at most 2^22 and an
 * n-element sum stays inside int32 for n <= 511.  No partial sum can leave the
 * range, so this is exact integer arithmetic and not arithmetic mod 2^32:
 * splitting a sum across four accumulators and adding them at the end cannot
 * change the result.  The group width is QT_GROUP and the widest row anyone
 * passes is 768, so the margin is large.  test_fixed.c proves the claim
 * against an int64 reference instead of assuming it.
 *
 * Four accumulators give the core four independent smlabb chains, so a 16-bit
 * multiply-add never waits on the previous one.
 *
 * All QT_GROUP terms are written out instead of looped.  The recorded device
 * build spends five instructions per MAC (ldrsb, ldrsh, cmp, smlabb, bne) --
 * that is a runtime-count loop that its compiler chose not to unroll -- and
 * whether a compiler unrolls a constant-count loop is a heuristic no source
 * file can bind.  Written out, every MAC is exactly its two loads and one
 * smlabb on every compiler, which is the floor for this data layout: 3
 * instructions per MAC instead of 5. */
static int32_t dot_group(const int8_t *w, const int16_t *x)
{
	int32_t p0 = 0, p1 = 0, p2 = 0, p3 = 0;

	p0 += (int32_t)w[0] * x[0];
	p1 += (int32_t)w[1] * x[1];
	p2 += (int32_t)w[2] * x[2];
	p3 += (int32_t)w[3] * x[3];
	p0 += (int32_t)w[4] * x[4];
	p1 += (int32_t)w[5] * x[5];
	p2 += (int32_t)w[6] * x[6];
	p3 += (int32_t)w[7] * x[7];
	p0 += (int32_t)w[8] * x[8];
	p1 += (int32_t)w[9] * x[9];
	p2 += (int32_t)w[10] * x[10];
	p3 += (int32_t)w[11] * x[11];
	p0 += (int32_t)w[12] * x[12];
	p1 += (int32_t)w[13] * x[13];
	p2 += (int32_t)w[14] * x[14];
	p3 += (int32_t)w[15] * x[15];
	p0 += (int32_t)w[16] * x[16];
	p1 += (int32_t)w[17] * x[17];
	p2 += (int32_t)w[18] * x[18];
	p3 += (int32_t)w[19] * x[19];
	p0 += (int32_t)w[20] * x[20];
	p1 += (int32_t)w[21] * x[21];
	p2 += (int32_t)w[22] * x[22];
	p3 += (int32_t)w[23] * x[23];
	p0 += (int32_t)w[24] * x[24];
	p1 += (int32_t)w[25] * x[25];
	p2 += (int32_t)w[26] * x[26];
	p3 += (int32_t)w[27] * x[27];
	p0 += (int32_t)w[28] * x[28];
	p1 += (int32_t)w[29] * x[29];
	p2 += (int32_t)w[30] * x[30];
	p3 += (int32_t)w[31] * x[31];
	return (p0 + p1) + (p2 + p3);
}

/* sum(w[i] * x[i]) over [0, n).  The shipping call is one weight group, taken
 * whole by dot_group; the trailing loop is the contract for any other length,
 * which is what test_fixed.c drives to prove the tiling has no off-by-one.
 *
 * Non-static so test_fixed.c can check it bit-exactly against a scalar
 * reference and bench_kernel.c can time it; both declare the prototype
 * locally rather than through fixed.h, which exists only for the device ABI.
 */
int32_t fixed_dot_q7_q12(const int8_t *w, const int16_t *x, int n)
{
	int32_t acc = 0;
	int i = 0;

	for (; i + QT_GROUP <= n; i += QT_GROUP)
		acc += dot_group(w + i, x + i);
	for (; i < n; i++)
		acc += (int32_t)w[i] * x[i];
	return acc;
}

static int64_t row_acc(const int8_t *w, const int32_t *s, const int16_t *x,
                       int cols)
{
	int64_t acc = 0;
	int groups = cols / QT_GROUP;

	for (int g = 0; g < groups; g++) {
		int32_t sc = s[g];

		acc += (int64_t)fixed_dot_q7_q12(w + (size_t)g * QT_GROUP,
		                                 x + (size_t)g * QT_GROUP,
		                                 QT_GROUP) * sc;
	}
	return acc;
}

void matvec_q(const int8_t *w, const int32_t *s, const int16_t *x, int16_t *y,
              int rows, int cols)
{
	int gpr = cols / QT_GROUP;

	for (int r = 0; r < rows; r++) {
		y[r] = sat16((row_acc(w, s, x, cols) + (1 << 23)) >> 24);
		w += cols;
		s += gpr;
	}
}

void matvec_q32(const int8_t *w, const int32_t *s, const int16_t *x, int32_t *y,
                int rows, int cols)
{
	int gpr = cols / QT_GROUP;

	for (int r = 0; r < rows; r++) {
		y[r] = sat32((row_acc(w, s, x, cols) + (1 << 23)) >> 24);
		w += cols;
		s += gpr;
	}
}

void rmsnorm_q(int16_t *o, const int16_t *x, const int16_t *w, int n,
               uint32_t rcp_n)
{
	uint64_t ss = 0;
	uint32_t mean, u, u2, y, r;
	int b, sh, extra;

	for (int i = 0; i < n; i++) {
		int32_t v = x[i];

		ss += (uint64_t)(v * v);
	}
	mean = (uint32_t)((ss * rcp_n) >> 31) + EPS_Q24;
	b = msb32(mean);
	sh = 24 - b + (b & 1);
	u = (sh >= 0) ? (mean << sh) : (mean >> -sh);
	u2 = u;
	extra = 0;
	if (u >= (1u << 25)) {
		u2 = u >> 1;
		extra = 1;
	}
	y = rsq_tab[(u2 >> 18) - 64];
	if (extra)
		y = (uint32_t)(((uint64_t)y * 46341) >> 16);
	for (int k = 0; k < 4; k++) {
		uint64_t e = ((uint64_t)u * (((uint64_t)y * y) >> 31)) >> 24;

		y = (uint32_t)(((uint64_t)y * ((3ULL << 31) - e)) >> 32);
	}
	r = y >> (15 - sh / 2);
	for (int i = 0; i < n; i++)
		o[i] = sat16(((int64_t)((int32_t)w[i] * x[i]) * r +
		              (1 << (QA + QR - 1))) >> (QA + QR));
}

int32_t exp_lut(int32_t dx)
{
	int32_t t, k, f, v;

	if (dx >= EXP_CLAMP)
		return 0;
	t = (dx * LOG2E_Q12) >> 12;
	k = t >> 14;
	f = t & 16383;
	v = (int32_t)exp_tab[f >> 6] -
	    ((((int32_t)exp_tab[f >> 6] - exp_tab[(f >> 6) + 1]) * (f & 63)) >> 6);
	return (k >= 15) ? 0 : (v >> k);
}

void softmax_q(const int32_t *sc, int n, int32_t *p)
{
	int32_t mx = sc[0];
	uint32_t sum = 0, sn, r, r15;
	int e;

	for (int t = 1; t < n; t++)
		if (sc[t] > mx)
			mx = sc[t];
	for (int t = 0; t < n; t++) {
		p[t] = exp_lut(mx - sc[t]);
		sum += (uint32_t)p[t];
	}
	e = msb32(sum);
	sn = sum << (31 - e);
	r = recip_tab[(sn >> 26) - 32];
	for (int k = 0; k < 4; k++) {
		uint32_t tt = (uint32_t)(((uint64_t)sn * r) >> 31);

		r = (uint32_t)(((uint64_t)r * ((1ULL << 32) - tt)) >> 31);
	}
	r15 = r >> 15;
	for (int t = 0; t < n; t++)
		p[t] = (int32_t)(((uint32_t)p[t] * r15) >> (1 + e));
}

void silu_q(int16_t *h, int n)
{
	for (int i = 0; i < n; i++) {
		int32_t x = h[i];
		int32_t xi = x + 32768;
		int32_t idx = xi >> 7;
		int32_t sig = (int32_t)silu_tab[idx] +
		              (((int32_t)silu_tab[idx + 1] - silu_tab[idx]) *
		               (xi & 127) >> 7);

		h[i] = sat16((x * sig + (1 << (QS - 1))) >> QS);
	}
}

void rope_q(int16_t *v, int pos, int hd, const qstate_t *qs)
{
	int half = hd >> 1;
	const int16_t *c = qs->rcos + (size_t)pos * qs->half;
	const int16_t *s = qs->rsin + (size_t)pos * qs->half;

	for (int i = 0; i < half; i++) {
		int64_t a = v[i], b = v[i + half];

		v[i] = sat16((a * c[i] - b * s[i] + (1 << 13)) >> 14);
		v[i + half] = sat16((a * s[i] + b * c[i] + (1 << 13)) >> 14);
	}
}

static void attention_q(const model_t *m, const qstate_t *qs, qwork_t *w,
                        int l, int pos)
{
	int d = m->dim, hd = d / m->heads;
	size_t off = (size_t)l * m->seq * d;

	memset(w->acc, 0, (size_t)d * sizeof(int64_t));
	for (int h = 0; h < m->heads; h++) {
		const int16_t *qh = w->q + h * hd;
		int32_t mx = INT32_MIN;

		for (int t = 0; t <= pos; t++) {
			const int16_t *kh = w->k + off + (size_t)t * d + h * hd;
			int64_t dot = 0;

			for (int i = 0; i < hd; i++)
				dot += (int64_t)((int32_t)qh[i] * kh[i]);
			w->scores[t] = (int32_t)(((int64_t)dot * qs->rhd +
			                          (1 << (QA + QR - 1))) >> (QA + QR));
			if (w->scores[t] > mx)
				mx = w->scores[t];
		}
		softmax_q(w->scores, pos + 1, w->p);
		for (int t = 0; t <= pos; t++) {
			int32_t pv = w->p[t];
			const int16_t *vh = w->v + off + (size_t)t * d + h * hd;

			for (int i = 0; i < hd; i++)
				w->acc[h * hd + i] += (int64_t)pv * vh[i];
		}
		for (int i = 0; i < hd; i++)
			w->att[h * hd + i] = sat16((w->acc[h * hd + i] +
			                           (1 << (QS - 1))) >> QS);
	}
}

int forward_q(const model_t *m, const qstate_t *qs, qwork_t *w, int token,
              int pos, int32_t *logits)
{
	int d = m->dim, hd = d / m->heads;

	if (!qs || token < 0 || token >= m->vocab ||
	    pos < 0 || pos >= m->seq || pos >= qs->seq)
		return -1;

	const int8_t *er = m->tok_emb + (size_t)token * d;
	const int32_t *es = m->tok_emb_s + (size_t)token * (d / QT_GROUP);

	for (int i = 0; i < d; i++)
		w->x[i] = sat16(((int64_t)er[i] * es[i / QT_GROUP] +
		                (1 << (QT_SCALE_BITS - QA - 1))) >>
		               (QT_SCALE_BITS - QA));

	for (int l = 0; l < m->layers; l++) {
		size_t nd = (size_t)d * d, nh = (size_t)m->hidden * d;
		size_t nhy = (size_t)d * m->hidden;
		size_t off = (size_t)l * m->seq * d;

		rmsnorm_q(w->xn, w->x, qs->n1q + (size_t)l * d, d, qs->rcp_n);
		matvec_q(m->wq + l * nd, m->wq_s + l * nd / QT_GROUP, w->xn,
		          w->q, d, d);
		matvec_q(m->wk + l * nd, m->wk_s + l * nd / QT_GROUP, w->xn,
		          w->k + off + (size_t)pos * d, d, d);
		matvec_q(m->wv + l * nd, m->wv_s + l * nd / QT_GROUP, w->xn,
		          w->v + off + (size_t)pos * d, d, d);

		for (int h = 0; h < m->heads; h++) {
			rope_q(w->q + h * hd, pos, hd, qs);
			rope_q(w->k + off + (size_t)pos * d + h * hd, pos, hd, qs);
		}

		attention_q(m, qs, w, l, pos);
		matvec_q(m->wo + l * nd, m->wo_s + l * nd / QT_GROUP, w->att,
		          w->xb, d, d);
		for (int i = 0; i < d; i++)
			w->x[i] = sat16((int32_t)w->x[i] + w->xb[i]);

		rmsnorm_q(w->xn, w->x, qs->n2q + (size_t)l * d, d, qs->rcp_n);
		matvec_q(m->w1 + l * nh, m->w1_s + l * nh / QT_GROUP, w->xn,
		          w->hb, m->hidden, d);
		matvec_q(m->w3 + l * nh, m->w3_s + l * nh / QT_GROUP, w->xn,
		          w->hb2, m->hidden, d);
		silu_q(w->hb, m->hidden);
		for (int i = 0; i < m->hidden; i++)
			w->hb[i] = sat16(((int32_t)w->hb[i] * w->hb2[i] +
			                 (1 << (QA - 1))) >> QA);
		matvec_q(m->w2 + l * nhy, m->w2_s + l * nhy / QT_GROUP, w->hb,
		          w->xb, d, m->hidden);
		for (int i = 0; i < d; i++)
			w->x[i] = sat16((int32_t)w->x[i] + w->xb[i]);
	}

	rmsnorm_q(w->xn, w->x, qs->normq, d, qs->rcp_n);
	matvec_q32(m->tok_emb, m->tok_emb_s, w->xn, logits, m->vocab, d);
	return 0;
}

int qstate_init(qstate_t *qs, const model_t *m, int seq)
{
	int d = m->dim, hd = d / m->heads, half = hd / 2;

	memset(qs, 0, sizeof(*qs));
	if (seq < 1 || hd % 2 || d % QT_GROUP || m->heads < 1 || d % m->heads)
		return -1;
	qs->seq = seq;
	qs->half = half;
	qs->rcp_n = (uint32_t)(1ULL << 31) / (uint32_t)d;
	qs->rhd = (int32_t)(65536.0 / sqrt((double)hd) + 0.5);
	qs->n1q = malloc((size_t)m->layers * d * sizeof(int16_t));
	qs->n2q = malloc((size_t)m->layers * d * sizeof(int16_t));
	qs->normq = malloc((size_t)d * sizeof(int16_t));
	qs->rcos = malloc((size_t)seq * half * sizeof(int16_t));
	qs->rsin = malloc((size_t)seq * half * sizeof(int16_t));
	if (!qs->n1q || !qs->n2q || !qs->normq || !qs->rcos || !qs->rsin) {
		qstate_free(qs);
		return -1;
	}

	for (int l = 0; l < m->layers; l++)
		for (int i = 0; i < d; i++) {
			float g = m->n1[(size_t)l * d + i];
			float g2 = m->n2[(size_t)l * d + i];

			qs->n1q[(size_t)l * d + i] =
			    sat16((int64_t)(g * (1 << QA) + (g >= 0 ? 0.5f : -0.5f)));
			qs->n2q[(size_t)l * d + i] =
			    sat16((int64_t)(g2 * (1 << QA) + (g2 >= 0 ? 0.5f : -0.5f)));
		}
	for (int i = 0; i < d; i++) {
		float g = m->norm[i];

		qs->normq[i] = sat16((int64_t)(g * (1 << QA) + (g >= 0 ? 0.5f : -0.5f)));
	}
	for (int p = 0; p < seq; p++)
		for (int i = 0; i < half; i++) {
			double ang = (double)p * pow(10000.0, -(double)i / (double)half);
			double c = cos(ang), s = sin(ang);

			qs->rcos[(size_t)p * half + i] =
			    (int16_t)(c * 16384.0 + (c >= 0 ? 0.5 : -0.5));
			qs->rsin[(size_t)p * half + i] =
			    (int16_t)(s * 16384.0 + (s >= 0 ? 0.5 : -0.5));
		}

	for (int j = 0; j < 64; j++)
		rsq_tab[j] = (uint32_t)(2147483648.0 /
		                        sqrt(1.0 + (j + 0.5) / 64.0) + 0.5);
	for (int j = 0; j < 257; j++)
		exp_tab[j] = (uint16_t)(32768.0 * exp2(-(double)j / 256.0) + 0.5);
	for (int j = 0; j < 513; j++) {
		double x = (double)j / 32.0 - 8.0;

		silu_tab[j] = (uint16_t)(32768.0 / (1.0 + exp(-x)) + 0.5);
	}
	for (int j = 0; j < 32; j++)
		recip_tab[j] = (uint32_t)(2147483648.0 /
		                          (1.0 + (j + 0.5) / 32.0) + 0.5);
	return 0;
}

void qstate_free(qstate_t *qs)
{
	if (!qs)
		return;
	free(qs->n1q); free(qs->n2q); free(qs->normq);
	free(qs->rcos); free(qs->rsin);
	memset(qs, 0, sizeof(*qs));
}

int qwork_alloc(qwork_t *qw, const model_t *m)
{
	size_t d = (size_t)m->dim;

	memset(qw, 0, sizeof(*qw));
	qw->x = malloc(d * sizeof(int16_t));
	qw->xb = malloc(d * sizeof(int16_t));
	qw->xn = malloc(d * sizeof(int16_t));
	qw->q = malloc(d * sizeof(int16_t));
	qw->att = malloc(d * sizeof(int16_t));
	qw->hb = malloc((size_t)m->hidden * sizeof(int16_t));
	qw->hb2 = malloc((size_t)m->hidden * sizeof(int16_t));
	qw->k = malloc((size_t)m->layers * m->seq * d * sizeof(int16_t));
	qw->v = malloc((size_t)m->layers * m->seq * d * sizeof(int16_t));
	qw->scores = malloc((size_t)m->seq * sizeof(int32_t));
	qw->p = malloc((size_t)m->seq * sizeof(int32_t));
	qw->acc = malloc(d * sizeof(int64_t));
	if (!qw->x || !qw->xb || !qw->xn || !qw->q || !qw->att || !qw->hb ||
	    !qw->hb2 || !qw->k || !qw->v || !qw->scores || !qw->p || !qw->acc) {
		qwork_free(qw);
		return -1;
	}
	return 0;
}

void qwork_free(qwork_t *qw)
{
	if (!qw)
		return;
	free(qw->x); free(qw->xb); free(qw->xn); free(qw->q); free(qw->att);
	free(qw->hb); free(qw->hb2); free(qw->k); free(qw->v);
	free(qw->scores); free(qw->p); free(qw->acc);
	memset(qw, 0, sizeof(*qw));
}
