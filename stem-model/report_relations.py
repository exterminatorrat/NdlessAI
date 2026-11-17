"""Per-relation wording accuracy, so "the model is weak on wording" has a list.

The aggregate number hides which relations carry the deficit: hand-written
accuracy has sat between 71.8% and 73.8% across 84, 90, 102 and 136 relations,
which is what an average looks like when a handful of relations fail and most
are fine. This script prints the per-relation table for the corpus's honest
split (the `eval` rows, which the model never trains on) and can fail a build
when any relation with enough rows falls below a floor.

    python3 report_relations.py                  # the table
    python3 report_relations.py --floor 0.5      # exit 1 under the floor
    python3 report_relations.py --course sat-act # one Tier 1 course
    python3 report_relations.py --course-floor   # per-course floors, as a gate

The questions go through `runtime/main_device`, which is one process for the
whole batch and the same pipeline the calculator runs.
"""

import argparse
import collections
import json
import os
import subprocess
import sys

import courses
from evaluate_paraphrase import load_corpora
from gen_traces import render_call

DEFAULT_DATA = "data/held_out.jsonl"


def run_batch(binary, weights, seq, questions):
    """One process, one line per question: answer<TAB>call."""

    payload = "\n".join(questions) + "\n"
    env = {"PATH": "/usr/bin:/bin", "NSM_QUIET": "1"}
    out = subprocess.run([binary, weights, str(seq)], input=payload,
                         capture_output=True, text=True, env=env).stdout
    calls = []
    for line in out.splitlines():
        calls.append(line.partition("\t")[2])
    while len(calls) < len(questions):
        calls.append("")
    return calls[: len(questions)]


def report(rows, args):
    """rows: [(relation, question, expected)]"""

    calls = run_batch(args.binary, args.weights, args.seq,
                      [q for _, q, _ in rows])
    per = collections.defaultdict(lambda: [0, 0])
    misses = []
    for (relation, question, expected), got in zip(rows, calls):
        hit = got == expected
        per[relation][0] += 1
        per[relation][1] += hit
        if not hit:
            misses.append((relation, question, expected, got))

    total = sum(v[0] for v in per.values())
    exact = sum(v[1] for v in per.values())
    print(f"{total} questions over {len(per)} relations: "
          f"{exact / total:.1%} exact call")
    print()

    worst = sorted(per.items(), key=lambda kv: (kv[1][1] / kv[1][0], -kv[1][0]))
    print(f"{'relation':<24}{'correct':>8}{'of':>5}{'exact':>9}")
    thin = 0
    for relation, (n, hits) in worst:
        flag = ""
        if n >= args.min_rows and hits / n < args.floor:
            flag = "  below floor"
            thin += 1
        print(f"{relation:<24}{hits:>8}{n:>5}{hits / n:>9.1%}{flag}")

    print()
    print(f"{len(per)} relations, {thin} below {args.floor:.0%} on "
          f"{args.min_rows}+ rows")

    if args.show_misses:
        print()
        for relation, question, expected, got in misses[: args.show_misses]:
            print(f"  {relation}\n    Q: {question}\n"
                  f"    want {expected}\n    got  {got or '(nothing)'}")
    return thin


def arg_audit(rows, args):
    """Which slot fails, and did the wrong value come from the sentence?

    An answer can be wrong three ways: the wrong relation, the right relation
    with the wrong numbers, or nothing at all. The middle one is where the
    wording work lives, and counting it per relation per argument position is
    the difference between "this relation is weak" and "this relation keeps
    putting the mass where the volume goes".
    """

    calls = run_batch(args.binary, args.weights, args.seq,
                      [q for _, q, _ in rows])
    per = collections.defaultdict(lambda: collections.Counter())
    slots = collections.defaultdict(lambda: collections.Counter())
    for (relation, question, expected), got in zip(rows, calls):
        if got == expected:
            per[relation]["correct"] += 1
            continue
        want_parts = expected.split()
        got_parts = got.split()
        if len(got_parts) < 2 or got_parts[1] != relation:
            per[relation]["wrong relation"] += 1
            continue
        want = want_parts[2:]
        have = got_parts[2:]
        per[relation]["wrong numbers"] += 1
        for i in range(max(len(want), len(have))):
            a = want[i] if i < len(want) else "-"
            b = have[i] if i < len(have) else "-"
            if a == b:
                continue
            slots[relation][i] += 1
            # A value the question literally contains is a number picked from
            # the wrong slot; one that is not is a number invented outright.
            if b != "-" and b.lstrip("-") in question:
                per[relation]["picked from text"] += 1
            else:
                per[relation]["invented"] += 1

    print(f"{'relation':<24}{'correct':>8}{'wrong num':>11}{'wrong rel':>11}"
          f"{'from text':>11}{'invented':>10}")
    for relation, counts in sorted(per.items(),
                                   key=lambda kv: -kv[1]["wrong numbers"]):
        if not counts["wrong numbers"]:
            continue
        bad = slots[relation]
        where = " ".join(f"a{i}:{n}" for i, n in sorted(bad.items()))
        print(f"{relation:<24}{counts['correct']:>8}"
              f"{counts['wrong numbers']:>11}{counts['wrong relation']:>11}"
              f"{counts['picked from text']:>11}{counts['invented']:>10}"
              f"   {where}")
    total = collections.Counter()
    for counts in per.values():
        total.update(counts)
    print()
    print(f"{total['correct']} correct, {total['wrong numbers']} right relation "
          f"with wrong numbers ({total['picked from text']} of those arguments "
          f"came from the sentence, {total['invented']} did not), "
          f"{total['wrong relation']} wrong relation")
    print()


def corpus_rows(args):
    """The eval split of the paraphrase corpus, as (relation, question, call)."""

    rows = []
    for row in load_corpora(args.corpus or "fixtures/paraphrase_*.jsonl"):
        if row["split"] != args.split:
            continue
        rows.append((row["relation"], row["question"],
                     render_call(row["relation"], row["args"])))
    return rows


def course_rows(rows, code):
    """The rows whose relation serves one course.

    A relation may serve several courses, so a row can belong to more than one
    course and is counted in each. That is deliberate: the question a course
    floor answers is "how does the program do on the questions this course
    asks", not "how many rows are exclusive to it".
    """

    return [row for row in rows if code in courses.courses_for(row[0])]


def floor_rows(args):
    """The rows the course floors were recorded on.

    A floor is a regression test against a fixed set of hand-written eval rows,
    so scoring it over whatever the corpus has grown into measures the corpus,
    not the program: the r6 build read 58.5% over a corpus that had gained 2,697
    generated rows and 71.4% over the calibrated 2,150. The default is therefore
    the calibrated set, and a drifted set is scored only when it is asked for.
    """

    if args.source == "held-out":
        if not args.allow_drift:
            raise SystemExit(
                "the course floors are hand-written accuracy; scoring generated "
                "rows against them needs --allow-drift")
        return held_out_rows(args)

    if not args.corpus:
        rows = []
        for name in courses.FLOOR_CORPUS:
            for row in load_corpora(os.path.join("fixtures", name)):
                if row["split"] == args.split:
                    rows.append((row["relation"], row["question"],
                                 render_call(row["relation"], row["args"])))
        print(f"course floors: the calibrated set, {len(rows)} eval rows from "
              f"{len(courses.FLOOR_CORPUS)} hand-written files")
        return rows

    rows = corpus_rows(args)
    if len(rows) != courses.FLOOR_EVAL_ROWS and not args.allow_drift:
        raise SystemExit(
            f"the floors were recorded on {courses.FLOOR_EVAL_ROWS} eval rows; "
            f"this corpus has {len(rows)}, so the number would not be a floor. "
            f"Pass --allow-drift to score it anyway, or leave --corpus off to "
            f"score the calibrated set.")
    return rows


def course_floors(rows, args):
    """Per-course accuracy against the recorded floors in courses.py.

    A floor is the number the corpus measured when the row was written, so it is
    a regression test, not a target. A course with too few scored rows is
    reported as such rather than judged: a percentage over eleven questions is a
    coin flip wearing a decimal point.
    """

    calls = run_batch(args.binary, args.weights, args.seq,
                      [q for _, q, _ in rows])
    per = collections.defaultdict(lambda: [0, 0])
    for (relation, _, expected), got in zip(rows, calls):
        for code in courses.courses_for(relation):
            per[code][0] += 1
            per[code][1] += got == expected

    total = sum(n for n, _ in per.values())
    print(f"{total} scored rows across {len(per)} Tier 1 courses; a row counts "
          f"in every course its relation serves")
    print()
    print(f"{'course':<14}{'name':<42}{'correct':>8}{'of':>6}"
          f"{'exact':>8}{'floor':>8}  status")
    failed = 0
    for code, course in courses.COURSES.items():
        n, hits = per.get(code, (0, 0))
        pct = 100.0 * hits / n if n else 0.0
        floor = course.exact_floor
        if n < courses.MIN_ROWS:
            status = "too few rows"
            shown = "-"
        elif floor is None:
            status = "no floor recorded"
            shown = "-"
        else:
            shown = f"{floor:.1f}%"
            ok = pct >= floor - courses.FLOOR_SLACK
            status = "PASS" if ok else "FAIL"
            failed += 0 if ok else 1
        print(f"{code:<14}{course.name[:40]:<42}{hits:>8}{n:>6}"
              f"{pct:>7.1f}%{shown:>8}  {status}")

    print()
    if failed:
        print(f"{failed} course(s) more than {courses.FLOOR_SLACK:.0f} points "
              f"below the recorded floor")
        return 1
    return 0


def held_out_rows(args):
    rows = []
    with open(args.data) as fh:
        for line in fh:
            row = json.loads(line)
            rows.append((row["relation"], row["question"], row["call"]))
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--source", choices=("held-out", "paraphrase"),
                    default="paraphrase",
                    help="held-out generated rows, or the hand-written corpus")
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--corpus", default=None,
                    help="corpus glob to score; defaults to all of it, or to "
                         "the calibrated set under --course-floor")
    ap.add_argument("--split", default="eval",
                    help="corpus split to score: eval is the honest one")
    ap.add_argument("--weights", default="weights.bin")
    ap.add_argument("--binary", default="runtime/main_device")
    ap.add_argument("--seq", type=int, default=160)
    ap.add_argument("--floor", type=float, default=0.5)
    ap.add_argument("--max-thin", type=int, default=0,
                    help="fail when more than this many relations sit below "
                         "the floor; the gate's recorded budget")
    ap.add_argument("--min-rows", type=int, default=4,
                    help="a relation needs this many rows before the floor "
                         "judges it")
    ap.add_argument("--show-misses", type=int, default=0)
    ap.add_argument("--arg-audit", action="store_true",
                    help="for the right relation with the wrong numbers, which "
                         "argument went wrong and where the wrong value came "
                         "from (W20's measurement)")
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--course", default="",
                    help="score only the rows whose relation serves this Tier 1 "
                         "course; codes are in courses.py")
    ap.add_argument("--course-floor", action="store_true",
                    help="per-course floors recorded in courses.py; exits 1 "
                         "when a course is more than FLOOR_SLACK below its own")
    ap.add_argument("--allow-drift", action="store_true",
                    help="score the floors on a set other than the one they "
                         "were recorded on; the number is then not a floor")
    args = ap.parse_args()

    for path in (args.binary, args.weights):
        if not os.path.exists(path):
            raise SystemExit(f"{path} missing; run `make -C runtime` first")

    if args.course_floor:
        rows = floor_rows(args)
    else:
        rows = held_out_rows(args) if args.source == "held-out" else corpus_rows(args)
    if args.limit:
        rows = rows[: args.limit]
    if not rows:
        raise SystemExit("no rows to score")

    if args.course_floor:
        return course_floors(rows, args)

    if args.course:
        rows = course_rows(rows, args.course)
        if not rows:
            raise SystemExit(f"no corpus rows for course {args.course!r}; "
                             f"codes are {', '.join(courses.COURSES)}")
        print(f"course {args.course}: {len(rows)} rows")

    if args.arg_audit:
        arg_audit(rows, args)

    thin = report(rows, args)
    if thin > args.max_thin:
        print(f"\n{thin} relations below the floor, budget is {args.max_thin}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
