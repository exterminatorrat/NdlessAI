#!/bin/sh
# One command that answers "is the recorded state real?".
#
# Runs every gate from TIER1_PLAN.md section 6 and prints PASS, FAIL or SKIP
# per gate. Exit code is 0 only if no gate fails; a gate that the host cannot
# run exits 77 and reports SKIP rather than FAIL.
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
    "$@" >/tmp/gate.$$ 2>&1
    rc=$?
    if [ "$rc" -eq 0 ]; then
        pass "$name"
    elif [ "$rc" -eq 77 ]; then
        # 77 is the documented "this host cannot run the gate" code, used by
        # crosscheck.sh when the cross compiler is absent. It is not a failure
        # and must not be reported as one.
        skip "$name"
    else
        failg "$name"
        tail -5 /tmp/gate.$$ | sed 's/^/      /'
    fi
}

# The documented workflow drives Python through uv; a python3 that already has
# the packages is equivalent. Prefer uv when it exists so no new environment is
# introduced, and fall back rather than reporting a false failure.
if command -v uv >/dev/null 2>&1; then
    PY="uv run --with pytest --with torch --with numpy --with sympy python"
else
    PY="python3"
    printf 'note  uv not found; using python3\n'
fi

echo "=== C gates ==="
if [ -d runtime ]; then
    run "runtime builds"         sh -c 'cd runtime && make -s'
    run "solvers match Python"   sh -c 'cd runtime && ./test_solvers | grep -q "0 mismatches"'
    run "tokenizer round-trip"   sh -c 'cd runtime && ./test_bpe | grep -q "failures: 0"'
    run "guard cases"            sh -c 'cd runtime && ./test_guard | grep -q "all cases passed"'
    run "call mask"              sh -c "cd runtime && ./test_callmask | grep -q 'all cases passed' && cd .. && $PY export_relations.py --check >/dev/null"
    run "graph"                  sh -c 'cd runtime && make -s test_graph >/dev/null 2>&1 && ./test_graph | grep -q "all checks passed"'
    run "typed answers"          sh -c 'cd runtime && ./test_typed | grep -q "all cases passed"'
    run "linear algebra"         sh -c 'cd runtime && ./test_matrix | grep -q "all cases passed"'
    run "tokenizer fixture sync" sh -c "$PY gen_bpe_cases.py --check >/dev/null"
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
if $PY -c 'import pytest, torch, numpy, sympy' >/dev/null 2>&1; then
    run "python tests" $PY -m pytest tests -q
else
    skip "python tests (pytest, torch, numpy or sympy missing)"
fi

echo "=== tokenizer sync ==="
if [ -f data/tokenizer.json ]; then
    run "bpe.h matches tokenizer" $PY check_tokenizer_sync.py
else
    skip "bpe.h matches tokenizer (no data/tokenizer.json)"
fi

echo "=== accuracy gates ==="
if [ "$QUICK" = "quick" ]; then
    skip "held-out phrasing (quick mode)"
    skip "wrong relation (quick mode)"
elif [ -f weights.bin ] && [ -f data/held_out.jsonl ]; then
    out=$($PY evaluate.py --data data/held_out.jsonl --limit 1000 2>/dev/null | tail -4)
    echo "$out" | sed 's/^/      /'
    exact=$(echo "$out" | awk '/exact call/{print $4}' | tr -d '%')
    wrong=$(echo "$out" | awk '/wrong relation/{print $4}' | tr -d '%')
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
