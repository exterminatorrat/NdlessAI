/* Host microbenchmark for the int8 x int16 product-sum kernel in fixed.c.
 *
 * Times the loop this change replaced -- copied verbatim below as b_ref_group,
 * so the comparison is against real shipped code and not a strawman -- against
 * the dot_group() that fixed.c now uses, over identical random data.  The work
 * mirrors one recorded model's per-token matrix-vector pass: d=256, L=6,
 * hidden=768, vocab=512 over the same weight matrices forward_q touches.
 *
 * Read the ratio with the target in mind.  arm926ej-s is single-issue with no
 * SIMD and no FPU, so on the device the cost of this loop is its instruction
 * count; a desktop core retires three to four instructions per cycle and hides
 * loop overhead in its out-of-order window, which is why the ratio here is
 * much smaller than the instruction counts imply.  This is directional
 * evidence that the change removes work, not an arm926ej-s measurement.
 *
 * Not part of the device build: the calculator links fixed.c alone, and only
 * this host Makefile knows about this file.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "infer.h"

/* fixed.c is included, not linked, so that the kernel and the row loop that
 * calls it sit in one translation unit exactly as they do on the device.
 * Compiled separately the compiler has to leave a call per weight group in the
 * row loop, which is code the device build does not run. */
#include "fixed.c"

#define DIM 256
#define HID 768
#define VOCAB 512
#define LAYERS 6

#define REPEATS 20
#define TRIALS 9

typedef struct {
	int8_t *w;
	int32_t *s;
	int rows, cols;
	int copies;
} b_mat_t;

/* The kernel this change replaced: one int32 accumulator per weight group and
 * one MAC per iteration, which the recorded build compiles to five
 * instructions per MAC (ldrsb, ldrsh, cmp, smlabb, bne). */
static int32_t b_ref_group(const int8_t *w, const int16_t *x)
{
	int32_t part = 0;

	for (int i = 0; i < QT_GROUP; i++)
		part += (int32_t)w[i] * x[i];
	return part;
}

/* Row loops.  Both call a group kernel once per QT_GROUP elements, so neither
 * side gets a call deleted by inlining that the other keeps. */
static int64_t b_ref_row(const int8_t *w, const int32_t *s, const int16_t *x,
                         int cols)
{
	int64_t acc = 0;
	int groups = cols / QT_GROUP;

	for (int g = 0; g < groups; g++)
		acc += (int64_t)b_ref_group(w + (size_t)g * QT_GROUP,
		                            x + (size_t)g * QT_GROUP) * s[g];
	return acc;
}

static int64_t b_new_row(const int8_t *w, const int32_t *s, const int16_t *x,
                         int cols)
{
	int64_t acc = 0;
	int groups = cols / QT_GROUP;

	for (int g = 0; g < groups; g++)
		acc += (int64_t)dot_group(w + (size_t)g * QT_GROUP,
		                          x + (size_t)g * QT_GROUP) * s[g];
	return acc;
}

static volatile int64_t b_sink;

static int64_t b_pass_ref(const b_mat_t *m, int nm, const int16_t *x)
{
	int64_t acc = 0;

	for (int k = 0; k < nm; k++) {
		int gpr = m[k].cols / QT_GROUP;

		for (int c = 0; c < m[k].copies; c++)
			for (int r = 0; r < m[k].rows; r++)
				acc += b_ref_row(m[k].w + (size_t)r * m[k].cols,
				                 m[k].s + (size_t)r * gpr, x,
				                 m[k].cols);
	}
	return acc;
}

static int64_t b_pass_new(const b_mat_t *m, int nm, const int16_t *x)
{
	int64_t acc = 0;

	for (int k = 0; k < nm; k++) {
		int gpr = m[k].cols / QT_GROUP;

		for (int c = 0; c < m[k].copies; c++)
			for (int r = 0; r < m[k].rows; r++)
				acc += b_new_row(m[k].w + (size_t)r * m[k].cols,
				                 m[k].s + (size_t)r * gpr, x,
				                 m[k].cols);
	}
	return acc;
}

typedef int64_t (*b_pass_fn)(const b_mat_t *, int, const int16_t *);

static double b_now(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

/* Best of TRIALS, each REPEATS passes.  The minimum is the least noisy
 * estimator for a kernel that only ever loses time to interrupts. */
static double b_best_ns(b_pass_fn fn, const b_mat_t *m, int nm, const int16_t *x,
                        double mac_pass)
{
	double best = 1e30;

	b_sink = fn(m, nm, x);
	for (int t = 0; t < TRIALS; t++) {
		double t0 = b_now();
		int64_t acc = 0;

		for (int r = 0; r < REPEATS; r++)
			acc += fn(m, nm, x);
		double dt = b_now() - t0;

		b_sink += acc;
		if (dt * 1e9 / (mac_pass * REPEATS) < best)
			best = dt * 1e9 / (mac_pass * REPEATS);
	}
	return best;
}

static uint32_t b_rng = 0x9e3779b9u;

static uint32_t b_rand(void)
{
	b_rng ^= b_rng << 13;
	b_rng ^= b_rng >> 17;
	b_rng ^= b_rng << 5;
	return b_rng;
}

int main(void)
{
	b_mat_t m[4] = {
		{NULL, NULL, VOCAB, DIM, 1},
		{NULL, NULL, DIM, DIM, 4 * LAYERS},
		{NULL, NULL, HID, DIM, 2 * LAYERS},
		{NULL, NULL, DIM, HID, LAYERS},
	};
	const int nm = (int)(sizeof(m) / sizeof(m[0]));
	int16_t x[HID];
	double mac_pass = 0.0;
	int64_t a, b;

	for (int i = 0; i < HID; i++)
		x[i] = (int16_t)((int32_t)(b_rand() >> 17) - 16384);

	for (int k = 0; k < nm; k++) {
		size_t nw = (size_t)m[k].rows * m[k].cols;
		size_t ns = (size_t)m[k].rows * (m[k].cols / QT_GROUP);

		m[k].w = malloc(nw);
		m[k].s = malloc(ns * sizeof(int32_t));
		if (!m[k].w || !m[k].s) {
			fprintf(stderr, "out of memory\n");
			return 1;
		}
		for (size_t i = 0; i < nw; i++)
			m[k].w[i] = (int8_t)(b_rand() >> 24);
		for (size_t i = 0; i < ns; i++)
			m[k].s[i] = 1 << 20;
		mac_pass += (double)nw * m[k].copies;
	}

	a = b_pass_ref(m, nm, x);
	b = b_pass_new(m, nm, x);
	printf("per-token matrix-vector work: %.0f MACs "
	       "(d=%d L=%d hidden=%d vocab=%d)\n",
	       mac_pass, DIM, LAYERS, HID, VOCAB);
	printf("results agree: %s\n", a == b ? "yes" : "NO -- kernels disagree");

	double ns_ref = b_best_ns(b_pass_ref, m, nm, x, mac_pass);
	double ns_new = b_best_ns(b_pass_new, m, nm, x, mac_pass);

	printf("%-10s %10.3f ms/token %7.3f ns/MAC\n", "original",
	       ns_ref * mac_pass * 1e-6, ns_ref);
	printf("%-10s %10.3f ms/token %7.3f ns/MAC\n", "optimised",
	       ns_new * mac_pass * 1e-6, ns_new);
	printf("speedup: %.2fx\n", ns_ref / ns_new);
	printf("note: desktop CPU timing of scalar code (both vectorisers off, "
	       "since the device has none); directional only, not an "
	       "arm926ej-s measurement\n");

	for (int k = 0; k < nm; k++) {
		free(m[k].w);
		free(m[k].s);
	}
	return a == b ? 0 : 1;
}
