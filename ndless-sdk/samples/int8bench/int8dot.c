#include <string.h>
#include "int8dot.h"

int32_t dot_i8_ref(const int8_t *w, const int8_t *a, int n)
{
	int32_t acc = 0;

	for (int i = 0; i < n; i++)
		acc += (int32_t)w[i] * (int32_t)a[i];

	return acc;
}

int32_t dot_i8x4_i16(const uint32_t *w4, const int16_t *a, int n)
{
	int32_t acc = 0;
	int words = n >> 2;

	for (int i = 0; i < words; i++) {
		uint32_t p = w4[i];
		const int16_t *v = a + 4 * i;

		acc += (int32_t)(int8_t)(p & 0xFF) * v[0];
		acc += (int32_t)(int8_t)((p >> 8) & 0xFF) * v[1];
		acc += (int32_t)(int8_t)((p >> 16) & 0xFF) * v[2];
		acc += (int32_t)(int8_t)(p >> 24) * v[3];
	}

	for (int i = 4 * words; i < n; i++)
		acc += (int32_t)(int8_t)(w4[i >> 2] >> ((i & 3) * 8)) * a[i];

	return acc;
}

void pack_i8x4(const int8_t *src, uint32_t *dst, int n)
{
	int words = (n + 3) >> 2;

	for (int i = 0; i < words; i++) {
		int8_t b[4] = { 0, 0, 0, 0 };

		for (int j = 0; j < 4 && 4 * i + j < n; j++)
			b[j] = src[4 * i + j];

		memcpy(&dst[i], b, sizeof(b));
	}
}

void widen_i8(const int8_t *src, int16_t *dst, int n)
{
	for (int i = 0; i < n; i++)
		dst[i] = src[i];
}
