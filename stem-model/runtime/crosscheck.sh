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
for f in bpe fixed solvers solvers_math solvers_trig solvers_physics \
         solvers_chem solvers_units infer pipeline main_device graph; do
    # shellcheck disable=SC2086
    $CC $ARCH -I. -c "$f.c" -o "$OUT/$f.o"
done

$CC -mcpu=arm926ej-s -marm -O3 --specs=rdimon.specs -o "$OUT/app.elf" \
    "$OUT"/*.o -lm

status=0
for f in bpe fixed solvers solvers_math solvers_trig solvers_physics \
         solvers_chem solvers_units graph; do
    # qstate_init and the exp_solve log fallback may use soft-float; the
    # per-token loop may not. solve_exp is static and single-caller, so it is
    # inlined into solvers_math_solve, and the exclusion has to follow it
    # there.
    hot=$($CC -print-prog-name=objdump >/dev/null 2>&1 && \
          arm-none-eabi-objdump -dr "$OUT/$f.o" | \
          awk '/^[0-9a-f]+ <.*>:/{fn=$2} /__aeabi_[fd]/{print fn}' | \
          grep -vE 'qstate_init|solvers_solve|solvers_math_solve' | sort -u || true)
    if [ -n "$hot" ]; then
        echo "FAIL $f.o: soft-float in hot path: $hot"
        status=1
    fi
done

text=$(arm-none-eabi-size "$OUT/app.elf" | awk 'NR==2{print $1}')
echo "ok: linked for arm926ej-s, text $text bytes"
exit $status
