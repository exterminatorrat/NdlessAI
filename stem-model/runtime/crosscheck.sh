#!/bin/sh
# Compiles the on-device path for the calculator's CPU and fails if the hot
# loops pull in soft-float helpers.
#
# The calculator has no FPU, so a stray float in the decode loop costs
# thousands of cycles per operation. This catches that here rather than on
# hardware. Requires arm-none-eabi-gcc; exits 77 (skip) if absent.
set -e

CC=${CROSS_CC:-arm-none-eabi-gcc}
ARCH="-mcpu=arm926ej-s -marm -O3 -std=c11 -Wall -Wextra -Werror"
OUT=${TMPDIR:-/tmp}/nsm-arm-check

command -v "$CC" >/dev/null 2>&1 || { echo "skip: $CC not found"; exit 77; }

mkdir -p "$OUT"
# Every source file on the device path. A new one must be added here or the
# link below fails with undefined references, which is how expr.c was caught.
for f in bpe fixed solvers solvers_math equation solvers_trig solvers_physics \
         solvers_chem solvers_thermo solvers_em solvers_units solvers_optics solvers_waves solvers_fluids solvers_modern infer pipeline main_device graph diagram expr \
         callmask matrix shorthand steps screen ui; do
    # shellcheck disable=SC2086
    $CC $ARCH -I. -c "$f.c" -o "$OUT/$f.o"
done

$CC -mcpu=arm926ej-s -marm -O3 --specs=rdimon.specs -o "$OUT/app.elf" \
    "$OUT"/*.o -lm

# The Ndless user interface is compile-checked, not linked: libndls itself is a
# binary the SDK ships separately and this tree does not carry, so the calls
# into it can only be resolved on a machine with the real SDK. Compiling the
# sources here still checks every type, every API signature and every buffer,
# which is the part an author cannot check by reading. The UI objects go into
# their own directory so they cannot confuse the link above.
mkdir -p "$OUT/ui"
for f in ../device/*.c; do
    # shellcheck disable=SC2086
    $CC $ARCH -I../device -I../../ndless-sdk/include -I. \
        -c "$f" -o "$OUT/ui/$(basename "$f" .c).o"
done

status=0
for f in bpe fixed solvers solvers_math equation solvers_trig solvers_physics \
         solvers_chem solvers_thermo solvers_em solvers_units solvers_optics solvers_waves solvers_fluids solvers_modern graph diagram expr callmask matrix shorthand steps screen ui; do
    # Two named functions own their soft-float, and nothing else may: qstate_init
    # builds the rope and activation tables once at startup, and solve_exp's
    # fallback for an exponent that is not a rational power of the base renders
    # four significant digits after the integer path has already failed. The
    # per-token loop may not. solve_exp is marked noinline so it cannot be folded
    # into solvers_math_solve and hide there under another name.
    hot=$($CC -print-prog-name=objdump >/dev/null 2>&1 && \
          arm-none-eabi-objdump -dr "$OUT/$f.o" | \
          awk '/^[0-9a-f]+ <.*>:/{fn=$2} /__aeabi_[fd]/{print fn}' | \
          grep -vE 'qstate_init|solve_exp' | sort -u || true)
    if [ -n "$hot" ]; then
        echo "FAIL $f.o: soft-float in hot path: $hot"
        status=1
    fi
done

text=$(arm-none-eabi-size "$OUT/app.elf" | awk 'NR==2{print $1}')
echo "ok: linked for arm926ej-s, text $text bytes"
exit $status
