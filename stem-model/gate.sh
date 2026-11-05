#!/bin/sh
# One command that answers "is the recorded state real?".
#
# Runs every gate from TIER1_PLAN.md section 6 and prints PASS or FAIL per
# gate. Exit code is 0 only if all gates pass.
#
# Usage:  sh stem-model/gate.sh          # all gates
#         sh stem-model/gate.sh quick    # skip the slow training-dependent ones
set -u

cd "$(dirname "$0")" || exit 1
QUICK=${1:-}
fail=0

pass() { printf 'PASS  %s\n' "$1"; }
failg() { printf 'FAIL  %s\n' "$1"; fail=1; }
skip() { printf 'SKIP  %s\n' "$1"; }

run() {
    name=$1
    shift
    if "$@" >/tmp/gate.$$ 2>&1; then
        pass "$name"
    else
        failg "$name"
        tail -5 /tmp/gate.$$ | sed 's/^/      /'
    fi
}

echo "=== C gates ==="
if [ -d runtime ]; then
    ( cd runtime && make -s >/dev/null 2>&1 ) || failg "runtime builds"
    run "solvers match Python"   sh -c 'cd runtime && ./test_solvers | grep -q "0 mismatches"'
    run "tokenizer round-trip"   sh -c 'cd runtime && ./test_bpe | grep -q "failures: 0"'
    run "guard cases"            sh -c 'cd runtime && ./test_guard | grep -q "all cases passed"'
    run "graph"                  sh -c 'cd runtime && make -s test_graph >/dev/null 2>&1 && ./test_graph | grep -q "all checks passed"'
    if [ -f weights.bin ]; then
        run "fixed-point parity" sh -c 'cd runtime && ./test_fixed ../weights.bin | grep -q "6/6 cases passed"'
    else
        skip "fixed-point parity (no weights.bin)"
    fi
    run "ARM cross-compile"      sh -c 'cd runtime && sh crosscheck.sh'
else
    failg "runtime directory missing"
fi

echo "=== Python gates ==="
run "python tests" uv run --with pytest --with torch --with numpy --with sympy \
    python -m pytest tests -q

echo "=== tokenizer sync ==="
if [ -f data/tokenizer.json ]; then
    run "bpe.h matches tokenizer" uv run --with sympy python check_tokenizer_sync.py
else
    skip "bpe.h matches tokenizer (no data/tokenizer.json)"
fi

echo "=== accuracy gates ==="
if [ "$QUICK" = "quick" ]; then
    skip "held-out phrasing (quick mode)"
    skip "wrong relation (quick mode)"
elif [ -f weights.bin ] && [ -f data/held_out.jsonl ]; then
    out=$(uv run --with numpy --with sympy python evaluate.py \
          --data data/held_out.jsonl --limit 100 2>/dev/null | tail -4)
    echo "$out" | sed 's/^/      /'
    exact=$(echo "$out" | awk '/exact call/{print $3}' | tr -d '%')
    wrong=$(echo "$out" | awk '/wrong relation/{print $3}' | tr -d '%')
    if [ -n "$exact" ] && [ "${exact%.*}" -ge 80 ]; then
        pass "held-out phrasing >= 80%"
    else
        failg "held-out phrasing >= 80% (got ${exact:-?}%)"
    fi
    if [ -n "$wrong" ] && [ "${wrong%.*}" -le 5 ]; then
        pass "wrong relation <= 5%"
    else
        failg "wrong relation <= 5% (got ${wrong:-?}%)"
    fi
else
    skip "accuracy gates (no weights.bin or held_out.jsonl)"
fi

rm -f /tmp/gate.$$
echo
if [ "$fail" -eq 0 ]; then
    echo "ALL GATES PASS"
else
    echo "GATES FAILED"
fi
exit "$fail"
