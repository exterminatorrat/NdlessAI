/* Fixed-point forward pass for infer.c models (NSM1 int8 weights).
 *
 * Q-format contract:
 *   activations (x, xn, q, k, v, att, hb, hb2)  int16  Q12   range +/-8.0
 *   logits                                       int32  Q12
 *   attention scores                             int32  Q12   (measured up to +/-12)
 *   softmax weights                              Q15, sum ~= 32768
 *   RMSNorm gains                                int16  Q12
 *   rope cos/sin                                 int16  Q14
 *   weight group scales                          int32  Q24 (as stored in NSM1)
 *
 * exp_lut clamp: scores are passed as dx = max - sc >= 0 in Q12; dx >= 42598
 * (10.4, exp(-10.4) ~ 3.0e-5 < 2^-15) returns 0, flushing terms that cannot
 * move a Q15 softmax weight.
 *
 * The hot path is pure integer arithmetic: no libm, no malloc, no 64-bit
 * division.  qstate_init must run first; it builds the exp/silu/rsqrt/recip
 * tables and per-model rope tables (libm is allowed only there, as at model
 * load).  qwork_alloc is a host convenience; on device the caller passes
 * statically sized buffers.
 */
#ifndef FIXED_H
#define FIXED_H

#include <stdint.h>

#include "infer.h"

#define QA 12
#define QS 15
#define QR 16

typedef struct {
	int seq, half;
	int32_t rhd;
	uint32_t rcp_n;
	int16_t *n1q, *n2q, *normq;
	int16_t *rcos, *rsin;
} qstate_t;

typedef struct {
	int16_t *x, *xb, *xn, *q, *att, *hb, *hb2;
	int16_t *k, *v;
	int32_t *scores, *p;
	int64_t *acc;
} qwork_t;

extern uint32_t fixed_sat_count;

int qstate_init(qstate_t *qs, const model_t *m, int seq);
void qstate_free(qstate_t *qs);
int qwork_alloc(qwork_t *qw, const model_t *m);
void qwork_free(qwork_t *qw);

void matvec_q(const int8_t *w, const int32_t *s, const int16_t *x, int16_t *y,
              int rows, int cols);
void matvec_q32(const int8_t *w, const int32_t *s, const int16_t *x, int32_t *y,
                int rows, int cols);
void rmsnorm_q(int16_t *o, const int16_t *x, const int16_t *w, int n,
               uint32_t rcp_n);
int32_t exp_lut(int32_t dx);
void softmax_q(const int32_t *sc, int n, int32_t *p);
void silu_q(int16_t *h, int n);
void rope_q(int16_t *v, int pos, int hd, const qstate_t *qs);
int forward_q(const model_t *m, const qstate_t *qs, qwork_t *w, int token,
              int pos, int32_t *logits);

#endif
