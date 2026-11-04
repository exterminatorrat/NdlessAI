#!/bin/sh
# Live progress for ndless-sdk/toolchain/build_toolchain.sh, which goes quiet
# for long stretches while GCC compiles.
#
# Usage:  sh tools/watch_toolchain.sh &
#         sh .github/packaging/build.sh
#
# Prints one line every 15 s: the current phase, a spinner showing that
# compilation is still moving, and elapsed time.
set -u

TC=${1:-ndless-sdk/toolchain}
SPIN='|/-\'
i=0
start=$(date +%s)

phase() {
    if [ -f "$TC/.built_gdb" ]; then
        echo "done"
    elif [ -f "$TC/.built_gcc_step2" ]; then
        echo "gcc step 2 (final compiler)"
    elif [ -f "$TC/.built_newlib" ]; then
        echo "gcc step 2 (final compiler)"
    elif [ -f "$TC/.built_gcc_step1" ]; then
        echo "newlib (target C library)"
    elif [ -f "$TC/.built_binutils" ]; then
        echo "gcc step 1 (bootstrap compiler)"
    else
        echo "downloading sources or building binutils"
    fi
}

count() {
    # Number of compiler processes is the only reliable "is it moving" signal.
    ps -eo comm 2>/dev/null | grep -cE '^cc1(\+\+)?$'
}

# Rough cumulative minutes to reach each phase boundary, for a rough ETA.
# Calibrated from a 2-core build; a fast Mac finishes sooner. Labelled
# "rough" in the output so it is not mistaken for a measurement.
eta_total() {
    if [ -f "$TC/.built_gcc_step2" ] || [ -f "$TC/.built_newlib" ]; then
        echo 70
    elif [ -f "$TC/.built_gcc_step1" ]; then
        echo 60
    elif [ -f "$TC/.built_binutils" ]; then
        echo 50
    else
        echo 60
    fi
}

while :; do
    ch=$(printf '%s' "$SPIN" | cut -c $((i % 4 + 1)))
    elapsed=$(( $(date +%s) - start ))
    mins=$((elapsed / 60))
    secs=$((elapsed % 60))
    p=$(phase)
    if [ "$p" = "done" ]; then
        printf '\n[%02d:%02d] toolchain complete\n' "$mins" "$secs"
        exit 0
    fi
    n=$(count)
    total=$(eta_total)
    left=$((total - mins))
    [ "$left" -lt 0 ] && left=0
    printf '[%02d:%02d] %s %s  (%s compiling, ~%d min left)\n' \
        "$mins" "$secs" "$ch" "$p" "$n" "$left"
    i=$((i + 1))
    sleep 15
done
