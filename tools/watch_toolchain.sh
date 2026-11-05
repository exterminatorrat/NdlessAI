#!/bin/sh
# Live progress for ndless-sdk/toolchain/build_toolchain.sh, which goes quiet
# for long stretches while GCC compiles.
#
# Usage:  cd ~/Desktop/ndlessAI && sh tools/watch_toolchain.sh
#
# Works whether or not the build is already running. Prints one line every
# 15 s: phase, compiler activity, and how long ago a build file last changed.
set -u

TC=${1:-ndless-sdk/toolchain}
SPIN='|/-\'
i=0
start=$(date +%s)
prev=0

# Number of compiler processes actually running. pgrep matches the full
# command line, so it works whether comm reports a bare name (Linux) or a
# full path (macOS).
compilers() {
    n=$(pgrep -f 'cc1plus|cc1 |cc1$' 2>/dev/null | wc -l | tr -d ' ')
    echo "${n:-0}"
}

phase() {
    if [ -f "$TC/.built_gdb" ]; then
        echo "complete"
    elif [ -f "$TC/.built_gcc_step2" ] || [ -f "$TC/.built_newlib" ]; then
        echo "gcc step 2 (final compiler)"
    elif [ -f "$TC/.built_gcc_step1" ]; then
        echo "newlib (target C library)"
    elif [ -f "$TC/.built_binutils" ]; then
        echo "gcc step 1 (bootstrap compiler)"
    else
        echo "downloading or building binutils"
    fi
}

# Count of build artefacts written so far, across every component's build tree.
# Each stage builds in its own directory (build for binutils, build-gcc for
# GCC), so counting one directory would freeze the line at the wrong number for
# hours. The delta between ticks is the honest progress signal.
objects() {
    total=0
    for d in "$TC"/build "$TC"/build-*; do
        [ -d "$d" ] || continue
        n=$(find "$d" -type f 2>/dev/null | wc -l | tr -d ' ')
        total=$((total + ${n:-0}))
    done
    echo "$total"
}

running() {
    pgrep -f 'build_toolchain' >/dev/null 2>&1 && echo yes || echo no
}

while :; do
    ch=$(printf '%s' "$SPIN" | cut -c $((i % 4 + 1)))
    elapsed=$(( $(date +%s) - start ))
    mins=$((elapsed / 60))
    secs=$((elapsed % 60))
    p=$(phase)

    if [ "$p" = "complete" ]; then
        printf '\n[%02d:%02d] toolchain complete\n' "$mins" "$secs"
        exit 0
    fi

    n=$(compilers)
    objs=$(objects)
    delta=$((objs - prev))
    prev=$objs
    act=$(running)

    printf '[%02d:%02d] %s  %s  (%s compiling, %s files' \
        "$mins" "$secs" "$ch" "$p" "$n" "$objs"
    if [ "$delta" -gt 0 ]; then
        printf ', +%s new)\n' "$delta"
    elif [ "$act" = "no" ]; then
        printf ', build not running)\n'
    elif [ "$objs" -gt 0 ] && [ "$n" -eq 0 ]; then
        printf ', no new files - may be stalled)\n'
    else
        printf ')\n'
    fi
    i=$((i + 1))
    sleep 15
done
