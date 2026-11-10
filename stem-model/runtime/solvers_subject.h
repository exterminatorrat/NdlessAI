#ifndef SOLVERS_SUBJECT_H
#define SOLVERS_SUBJECT_H

#include <stddef.h>

/*
 * Internal boundary between solvers.c and the per-subject solver files.
 *
 * solvers_solve() walks the subject solvers in order.  Each one returns 1
 * when it recognised and handled the name, 0 when the name is not one of its
 * own, and a negative solvers_solve() rejection code when the name matched
 * but the call could not be handled.  Only the shared numeric machinery
 * below is visible across translation units; everything else stays static in
 * the file that uses it.
 */

typedef struct {
	long long n, d;
} rat;

#define WIDE_LIMIT 9000000000000000LL

extern const long long P10[];

long long gcd_ll(long long a, long long b);
int rat_norm(rat *r);
int rat_add(rat *r, const rat *a, const rat *b);
int rat_sub(rat *r, const rat *a, const rat *b);
int rat_mul(rat *r, const rat *a, const rat *b);
int rat_div(rat *r, const rat *a, const rat *b);
unsigned long long isqrt64(unsigned long long n);
long long div_round(long long a, long long b);
int pow10_index(long long d);
void out_set(char *out, size_t outlen, const char *buf);
int dec4_render(long long num, long long den, char *out, size_t outlen);
void fmt_rat(const rat *r, char *out, size_t outlen);
int parse_rat(const char *s, rat *r, int *flt);
long long sin9(long long tnum, int texp, int *rc);
long long sqrt12(long long n, long long d, int *rc);
long long log_core_9(long long num, long long den, long long *k_out,
                     int *rc);
long long log10_9(long long num, long long den, int *rc);
long long ln_9(long long num, long long den, int *rc);
long long exp_9(long long x9, int *rc);

typedef int (*solvers_subject_fn)(const char *name, const char *const *args,
                                  int nargs, char *out, size_t outlen);

int solvers_math_solve(const char *name, const char *const *args, int nargs,
                       char *out, size_t outlen);
int solvers_trig_solve(const char *name, const char *const *args, int nargs,
                       char *out, size_t outlen);
int solvers_physics_solve(const char *name, const char *const *args, int nargs,
                          char *out, size_t outlen);
int solvers_chem_solve(const char *name, const char *const *args, int nargs,
                       char *out, size_t outlen);
int solvers_thermo_solve(const char *name, const char *const *args,
                         int nargs, char *out, size_t outlen);
int solvers_em_solve(const char *name, const char *const *args, int nargs,
                     char *out, size_t outlen);
int solvers_units_solve(const char *name, const char *const *args, int nargs,
                        char *out, size_t outlen);

#endif
