#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "fixed.h"
#include "infer.h"

#define MAXLEN 40

typedef struct {
	int len;
	int tok[MAXLEN];
} case_t;

static int argmax32(const int32_t *v, int n)
{
	int best = 0;

	for (int i = 1; i < n; i++)
		if (v[i] > v[best])
			best = i;
	return best;
}

static double pearson(const float *a, const float *b, int n)
{
	double sa = 0, sb = 0, saa = 0, sbb = 0, sab = 0;

	for (int i = 0; i < n; i++) {
		sa += a[i];
		sb += b[i];
		saa += (double)a[i] * a[i];
		sbb += (double)b[i] * b[i];
		sab += (double)a[i] * b[i];
	}
	double cov = sab - sa * sb / n;
	double va = saa - sa * sa / n, vb = sbb - sb * sb / n;

	return cov / sqrt(va * vb);
}

static unsigned lcg = 12345;

static int next_tok(int vocab)
{
	lcg = lcg * 1103515245u + 12345u;
	return (int)((lcg >> 16) % (unsigned)vocab);
}

int main(int argc, char **argv)
{
	model_t m;
	qstate_t qs;
	qwork_t qw;
	float *fl, *ff;
	int32_t *fx;
	case_t cases[6];
	int nc = 0, fails = 0;
	double worst = 0;

	if (argc != 2) {
		fprintf(stderr, "usage: %s weights.bin\n", argv[0]);
		return 2;
	}
	if (model_load(argv[1], &m) < 0) {
		fprintf(stderr, "cannot load %s\n", argv[1]);
		return 1;
	}
	m.seq = MAXLEN;
	if (qstate_init(&qs, &m, MAXLEN) < 0 || qwork_alloc(&qw, &m) < 0) {
		fprintf(stderr, "fixed init failed\n");
		return 1;
	}
	fl = malloc((size_t)m.vocab * sizeof(float));
	ff = malloc((size_t)m.vocab * sizeof(float));
	fx = malloc((size_t)m.vocab * sizeof(int32_t));
	if (!fl || !ff || !fx) {
		fprintf(stderr, "out of memory\n");
		return 1;
	}

	cases[nc].len = 1;
	cases[nc].tok[0] = 7;
	nc++;
	cases[nc].len = 5;
	for (int i = 0; i < 5; i++)
		cases[nc].tok[i] = (int)((const int[]){259, 32, 261, 32, 258}[i]);
	nc++;
	cases[nc].len = 6;
	for (int i = 0; i < 6; i++)
		cases[nc].tok[i] = 3;
	nc++;
	cases[nc].len = 12;
	for (int i = 0; i < 12; i++)
		cases[nc].tok[i] = next_tok(m.vocab);
	nc++;
	cases[nc].len = 16;
	for (int i = 0; i < 16; i++)
		cases[nc].tok[i] = i;
	nc++;
	cases[nc].len = 32;
	for (int i = 0; i < 32; i++)
		cases[nc].tok[i] = next_tok(m.vocab);
	nc++;
	for (int c = 0; c < nc; c++)
		for (int i = 0; i < cases[c].len; i++)
			cases[c].tok[i] %= m.vocab;

	printf("weights %s  dim=%d layers=%d heads=%d hidden=%d vocab=%d\n",
	       argv[1], m.dim, m.layers, m.heads, m.hidden, m.vocab);
	printf("%-4s %4s %10s %9s %9s %9s %5s %5s\n", "case", "len", "maxdiff",
	       "corr", "argmax_f", "argmax_q", "pos", "sat");

	for (int c = 0; c < nc; c++) {
		state_t *fs = state_new(&m);

		if (!fs) {
			fprintf(stderr, "out of memory\n");
			return 1;
		}
		fixed_sat_count = 0;
		int pos_ok = 1;
		for (int p = 0; p < cases[c].len; p++) {
			int tok = cases[c].tok[p];

			forward(&m, fs, tok, p, fl);
			if (forward_q(&m, &qs, &qw, tok, p, fx) < 0) {
				fprintf(stderr, "forward_q failed\n");
				return 1;
			}
			if (argmax(fl, m.vocab) != argmax32(fx, m.vocab))
				pos_ok = 0;
		}
		double maxd = 0;
		for (int i = 0; i < m.vocab; i++) {
			ff[i] = (float)fx[i] / (float)(1 << QA);
			double dd = fabs((double)fl[i] - ff[i]);

			if (dd > maxd)
				maxd = dd;
		}
		double cr = pearson(fl, ff, m.vocab);
		int af = argmax(fl, m.vocab), aq = argmax32(fx, m.vocab);
		int ok = pos_ok && af == aq && cr > 0.999;

		if (maxd > worst)
			worst = maxd;
		if (!ok)
			fails++;
		printf("%-4d %4d %10.6f %9.6f %9d %9d %5s %5u %s\n", c + 1,
		       cases[c].len, maxd, cr, af, aq, pos_ok ? "ok" : "FLIP",
		       fixed_sat_count, ok ? "" : "  << FAIL");
		state_free(fs);
	}

	printf("worst maxdiff %.6f, %d/%d cases passed\n", worst,
	       nc - fails, nc);
	qwork_free(&qw);
	qstate_free(&qs);
	free(fl); free(ff); free(fx);
	model_free(&m);
	return fails ? 1 : 0;
}
