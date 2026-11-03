#ifndef INT8DOT_H
#define INT8DOT_H

#include <stdint.h>

int32_t dot_i8_ref(const int8_t *w, const int8_t *a, int n);
int32_t dot_i8x4_i16(const uint32_t *w4, const int16_t *a, int n);
void pack_i8x4(const int8_t *src, uint32_t *dst, int n);
void widen_i8(const int8_t *src, int16_t *dst, int n);

#endif
