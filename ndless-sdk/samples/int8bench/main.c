#include <libndls.h>
#include <stdio.h>
#include <stdlib.h>

#include "int8dot.h"

#define DIM 352
#define PARAMS 11000000
#define REPS 3
#define MACS_PER_TOKEN PARAMS

static uint32_t rng = 12345;
static volatile int32_t sink;

static uint32_t rnd(void)
{
	rng = rng * 1103515245u + 12345u;
	return rng >> 16;
}

static int32_t run_ref(const int8_t *w, const int8_t *a, int rows)
{
	int32_t acc = 0;

	for (int row = 0; row < rows; row++)
		acc += dot_i8_ref(w + (size_t)row * DIM, a, DIM);

	return acc;
}

static int32_t run_packed(const uint32_t *w4, const int16_t *a, int rows)
{
	int32_t acc = 0;

	for (int row = 0; row < rows; row++)
		acc += dot_i8x4_i16(w4 + (size_t)row * (DIM / 4), a, DIM);

	return acc;
}

int main(void)
{
	int8_t *w8 = malloc(PARAMS);
	uint32_t *w4 = malloc(PARAMS / 4 + 4);
	int8_t *a8 = malloc(DIM);
	int16_t *a16 = malloc(DIM * sizeof(int16_t));
	int rows = PARAMS / DIM;

	assert_ndless_rev(801);

	if (!w8 || !w4 || !a8 || !a16) {
		printf("alloc failed; max single alloc is about 21.5 MiB\n");
		wait_key_pressed();
		return 1;
	}

	for (int i = 0; i < PARAMS; i++)
		w8[i] = (int8_t)(rnd() & 0xFF);
	for (int i = 0; i < DIM; i++)
		a8[i] = (int8_t)(rnd() & 0xFF);

	pack_i8x4(w8, w4, PARAMS);
	widen_i8(a8, a16, DIM);

	printf("CX II: %s   dim %d   params %d\n", is_cx2 ? "yes" : "no", DIM, PARAMS);
	printf("kernel agree on 4 rows: %s\n",
		run_ref(w8, a8, 4) == run_packed(w4, a16, 4) ? "yes" : "NO");

	printf("\nclock() is stubbed in this SDK,\n");
	printf("so time each phase with a watch.\n\n");

	printf("Enter: reference kernel x%d\n", REPS);
	wait_key_pressed();
	for (int i = 0; i < REPS; i++)
		sink += run_ref(w8, a8, rows);
	printf("PHASE 1 DONE\n");
	wait_key_pressed();

	printf("Enter: packed kernel x%d\n", REPS);
	wait_key_pressed();
	for (int i = 0; i < REPS; i++)
		sink += run_packed(w4, a16, rows);
	printf("PHASE 2 DONE\n");

	printf("\ntokens/s = %d / seconds\n", REPS);
	printf("(one rep = %d MACs)\n", MACS_PER_TOKEN);
	wait_key_pressed();

	free(w8);
	free(w4);
	free(a8);
	free(a16);
	return 0;
}
