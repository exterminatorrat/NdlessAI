#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fixed.h"
#include "infer.h"

#define MAXLEN 40

#define FIXED_TIE_TOL 0.02

/* The int8 MAC kernel is not in fixed.h, which exists for the device ABI.
 * Declared here so this file is also the bit-exactness gate for it. */
extern int32_t fixed_dot_q7_q12(const int8_t *w, const int16_t *x, int n);

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

/* --- int8 x int16 kernel: bit-exactness against scalar references --- */

#define DOT_MAX 320

static uint32_t rng = 0x9e3779b9u;

static uint32_t next_rand(void)
{
	rng ^= rng << 13;
	rng ^= rng >> 17;
	rng ^= rng << 5;
	return rng;
}

/* Poison the elements just past a length: a kernel that reads past n changes
 * the sum instead of passing by luck.  DOT_MAX leaves room for the guard. */
static void poison_after(int8_t *w, int16_t *x, int n)
{
	for (int i = n; i < n + 8 && i < DOT_MAX; i++) {
		w[i] = INT8_MAX;
		x[i] = INT16_MAX;
	}
}

/* The sum done the obvious way, one term at a time, in int64.  Comparing the
 * kernel against an int64 sum rather than against another int32 loop is what
 * makes this a proof that reordering the terms is exact: the int64 value is
 * the right answer even if a partial sum had wrapped. */
static int64_t ref_dot64(const int8_t *w, const int16_t *x, int n)
{
	int64_t acc = 0;

	for (int i = 0; i < n; i++)
		acc += (int64_t)w[i] * x[i];
	return acc;
}

/* The row loop fixed.c shipped before the kernel change: one int32 accumulator
 * per weight group, one MAC per iteration. */
static int64_t ref_row_acc(const int8_t *w, const int32_t *s, const int16_t *x,
                           int cols)
{
	int64_t acc = 0;
	int groups = cols / QT_GROUP;

	for (int g = 0; g < groups; g++) {
		int32_t part = 0;

		for (int i = 0; i < QT_GROUP; i++)
			part += (int32_t)w[g * QT_GROUP + i] * x[g * QT_GROUP + i];
		acc += (int64_t)part * s[g];
	}
	return acc;
}

static int16_t ref_sat16(int64_t v)
{
	if (v > 32767)
		return 32767;
	if (v < -32768)
		return -32768;
	return (int16_t)v;
}

static int32_t ref_sat32(int64_t v)
{
	if (v > INT32_MAX)
		return INT32_MAX;
	if (v < INT32_MIN)
		return INT32_MIN;
	return (int32_t)v;
}

static void fill_group(int8_t *w, int16_t *x, int n, int regime)
{
	for (int i = 0; i < n; i++) {
		switch (regime) {
		case 0: /* full range on both sides */
			w[i] = (int8_t)(next_rand() >> 24);
			x[i] = (int16_t)(next_rand() >> 16);
			break;
		case 1: /* the extremes, so signs and magnitudes both flip */
			w[i] = (next_rand() & 1) ? INT8_MIN : INT8_MAX;
			x[i] = (next_rand() & 1) ? INT16_MIN : INT16_MAX;
			break;
		default: /* small values: catches a term added or dropped twice */
			w[i] = (int8_t)((int)(next_rand() % 9) - 4);
			x[i] = (int16_t)((int)(next_rand() % 33) - 16);
			break;
		}
	}
}

/* Every index must be counted exactly once.  With one non-zero term at a time
 * the expected sum is known by construction, which is what catches a tiling
 * error at a block or group boundary that random data would average away. */
static int check_kernel_every_index(void)
{
	int8_t w[DOT_MAX];
	int16_t x[DOT_MAX];
	int fails = 0, cases = 0;

	for (int n = 0; n <= 130; n++) {
		for (int j = 0; j < n; j++) {
			int32_t want, got;

			memset(w, 0, (size_t)n);
			memset(x, 0, (size_t)n * sizeof(int16_t));
			w[j] = (int8_t)(j % 2 ? -5 : 5);
			x[j] = (int16_t)(j * 7 - 40);
			poison_after(w, x, n);
			want = (int32_t)w[j] * x[j];
			got = fixed_dot_q7_q12(w, x, n);
			cases++;
			if (got != want) {
				fprintf(stderr,
				        "kernel: n=%d index=%d got %d want %d\n",
				        n, j, got, want);
				fails++;
			}
		}
	}
	printf("kernel every-index  %6d checks  %s\n", cases,
	       fails ? "FAIL" : "ok");
	return fails;
}

static int check_kernel_random(void)
{
	static const int lens[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 15, 16, 17,
	                            31, 32, 33, 63, 64, 65, 127, 128, 129, 255,
	                            256, 319, 320 };
	int8_t w[DOT_MAX];
	int16_t x[DOT_MAX];
	int fails = 0, cases = 0;

	for (size_t li = 0; li < sizeof(lens) / sizeof(lens[0]); li++) {
		int n = lens[li];

		for (int regime = 0; regime < 3; regime++)
			for (int round = 0; round < 8; round++) {
				int64_t want;
				int32_t got;

				fill_group(w, x, n, regime);
				poison_after(w, x, n);
				want = ref_dot64(w, x, n);
				got = fixed_dot_q7_q12(w, x, n);
				cases++;
				if (want < INT32_MIN || want > INT32_MAX) {
					fprintf(stderr,
					        "test data out of kernel contract: "
					        "n=%d sum=%lld\n", n, (long long)want);
					fails++;
					continue;
				}
				if (got != (int32_t)want) {
					fprintf(stderr,
					        "kernel: n=%d regime=%d got %d want %d\n",
					        n, regime, got, (int32_t)want);
					fails++;
				}
			}
	}
	printf("kernel random       %6d checks  %s\n", cases,
	       fails ? "FAIL" : "ok");
	return fails;
}

/* matvec_q/matvec_q32 must stay bit-identical to the row loop the kernel
 * replaced, including the +2^23 rounding, the shift and the clamp.  The clamp
 * here deliberately does not touch fixed_sat_count: this test compares values,
 * not saturation statistics.  scale 0 keeps results in range so the comparison
 * is sensitive to a kernel error; scale 1 overflows both outputs so the two
 * clamps are reached at all. */
static int check_matvec(void)
{
	static const int shapes[][3] = {
		{ 1, 32, 0 }, { 2, 64, 0 }, { 3, 96, 0 }, { 4, 160, 0 },
		{ 5, 256, 0 }, { 8, 512, 0 }, { 2, 64, 1 },
	};
	int8_t w[8 * 512];
	int32_t s[8 * 512 / QT_GROUP];
	int16_t x[512], y16[8];
	int32_t y32[8];
	int fails = 0, cases = 0;

	for (size_t si = 0; si < sizeof(shapes) / sizeof(shapes[0]); si++) {
		int rows = shapes[si][0], cols = shapes[si][1];

		for (int i = 0; i < rows * cols; i++)
			w[i] = (int8_t)(next_rand() >> 24);
		for (int i = 0; i < cols; i++)
			x[i] = (int16_t)(next_rand() >> 16);
		for (int i = 0; i < rows * (cols / QT_GROUP); i++)
			s[i] = shapes[si][2] ? (int32_t)(next_rand() >> 2)
			                     : (int32_t)(next_rand() >> 20) + 1;

		matvec_q(w, s, x, y16, rows, cols);
		matvec_q32(w, s, x, y32, rows, cols);
		for (int r = 0; r < rows; r++) {
			int64_t acc = ref_row_acc(w + (size_t)r * cols,
			                          s + (size_t)r * (cols / QT_GROUP),
			                          x, cols);
			int16_t want16 = ref_sat16((acc + (1 << 23)) >> 24);
			int32_t want32 = ref_sat32((acc + (1 << 23)) >> 24);

			cases++;
			if (y16[r] != want16) {
				fprintf(stderr,
				        "matvec_q: %dx%d row %d got %d want %d\n",
				        rows, cols, r, y16[r], want16);
				fails++;
			}
			if (y32[r] != want32) {
				fprintf(stderr,
				        "matvec_q32: %dx%d row %d got %d want %d\n",
				        rows, cols, r, y32[r], want32);
				fails++;
			}
		}
	}
	printf("matvec vs reference %6d checks  %s\n", cases,
	       fails ? "FAIL" : "ok");
	return fails;
}

static int check_kernel(void)
{
	int fails = 0;

	fails += check_kernel_every_index();
	fails += check_kernel_random();
	fails += check_matvec();
	printf("kernel bit-exactness: %s\n", fails ? "FAIL" : "ok");
	return fails;
}

int main(int argc, char **argv)
{
	model_t m;
	qstate_t qs;
	qwork_t qw;
	float *fl, *ff;
	int32_t *fx;
	case_t cases[6];
	int nc = 0, kfails, pfails = 0;
	double worst = 0;

	if (argc > 2) {
		fprintf(stderr, "usage: %s [weights.bin]\n", argv[0]);
		return 2;
	}

	kfails = check_kernel();

	if (argc < 2) {
		printf("skip fixed-point parity: no weights file given\n");
		return kfails ? 1 : 0;
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
		int pos_ok = 1, ties = 0;
		for (int p = 0; p < cases[c].len; p++) {
			int tok = cases[c].tok[p];

			forward(&m, fs, tok, p, fl);
			if (forward_q(&m, &qs, &qw, tok, p, fx) < 0) {
				fprintf(stderr, "forward_q failed\n");
				return 1;
			}
			int af_p = argmax(fl, m.vocab), aq_p = argmax32(fx, m.vocab);

			if (af_p != aq_p) {
				if (fabs((double)fl[af_p] - (double)fl[aq_p]) <= FIXED_TIE_TOL)
					ties++;
				else
					pos_ok = 0;
			}
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
			pfails++;
		printf("%-4d %4d %10.6f %9.6f %9d %9d %5s %5u %s\n", c + 1,
		       cases[c].len, maxd, cr, af, aq, pos_ok ? "ok" : "FLIP",
		       fixed_sat_count, ok ? "" : "  << FAIL");
		if (ties)
			printf("     %d near-tie position(s) within %.3f\n", ties,
			       FIXED_TIE_TOL);
		state_free(fs);
	}

	printf("worst maxdiff %.6f, %d/%d cases passed\n", worst,
	       nc - pfails, nc);
	qwork_free(&qw);
	qstate_free(&qs);
	free(fl); free(ff); free(fx);
	model_free(&m);
	return (kfails || pfails) ? 1 : 0;
}
