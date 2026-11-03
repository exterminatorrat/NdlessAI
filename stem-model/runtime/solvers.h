#ifndef SOLVERS_H
#define SOLVERS_H

#include <stddef.h>

/*
 * Dependency-free fixed-point solvers for the 11 STEM relations defined in
 * relations.py, targeted at the TI-Nspire (ARM926EJ-S, no FPU).
 *
 * solvers_solve(name, args, nargs, out, outlen) renders byte-identical
 * output to the Python solve() in relations.py for the same inputs.  args
 * are the call tokens produced by gen_traces.py render_call(), i.e. the
 * whitespace-separated fields after the relation name ("CALL ohm_i 2 3"
 * passes {"2", "3"}), exactly what run_call() would split and parse.
 *
 * Host vs device (ARM926EJ-S, soft-float, int8 model):
 *   - Everything is int64 / exact-rational fixed point: argument parsing,
 *     quad_solve (integer and irrational roots via an integer Newton
 *     isqrt), deriv_poly3, integr_poly3, linear2, exp_solve exact powers,
 *     the four-significant-digit dec() renderer, and sin() for proj_range
 *     (range reduction plus a Taylor series at scale 1e9) are all
 *     integer-only and device-safe.
 *   - exp_solve's fallback for k that is not a rational power of base
 *     uses log() from libm on doubles.  Python itself goes through
 *     floating point on that path (the log of a non-power never
 *     simplifies to an exact integer), so it is outside the generator's
 *     domain; a device build would need a soft-float log there or should
 *     surface the error.
 *
 * Supported numeric domain (values outside return an error; these match
 * what the Python generators can produce): decimal tokens with at most 9
 * fractional digits and magnitude < 1e6; exact-rational intermediates kept
 * under 2e9 by the rational ops.  Cases where the Python solver itself
 * raises (singular linear2 system, complex quad_solve roots) return an
 * error here instead of crashing; quad_solve with a==0 and b==0 renders ""
 * exactly like Python.
 *
 * Returns 0 and writes a NUL-terminated answer string on success; negative
 * on error with out set to "":
 *   -1 unknown relation    -2 wrong nargs    -3 malformed argument token
 *   -4 value outside the supported fixed-point domain
 * out is safely truncated (still NUL-terminated) if outlen is too small.
 */
int solvers_solve(const char *name, const char *const *args, int nargs,
                  char *out, size_t outlen);

/*
 * Four-significant-digit rendering of the exact rational num/den, matching
 * Python's f"{num/den:.4g}" whenever the double rounding of num/den agrees
 * with exact round-half-to-even (always true for the values the relations
 * emit; ties on binary-exact values such as x/32 round half to even).
 * Integers print bare ("13"), non-integers as plain decimals ("0.6667"),
 * trailing zeros stripped, scientific notation ("1e+04", "5e-05") outside
 * [1e-4, 1e4).  Exposed so the test binary can hammer it directly.
 * Returns 0 on success, -4 if den is 0 or the value is out of domain.
 */
int solvers_dec4(long long num, long long den, char *out, size_t outlen);

#endif
